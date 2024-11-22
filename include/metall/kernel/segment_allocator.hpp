// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_SEGMENT_ALLOCATOR_HPP
#define METALL_KERNEL_SEGMENT_ALLOCATOR_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <iomanip>
#include <limits>
#include <set>
#include <filesystem>

#include <metall/kernel/bin_number_manager.hpp>
#include <metall/kernel/bin_directory.hpp>
#include <metall/kernel/chunk_directory.hpp>
#include <metall/kernel/object_size_manager.hpp>
#include <metall/detail/char_ptr_holder.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/logger.hpp>

#ifndef METALL_DISABLE_CONCURRENCY
#define METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
#endif

#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
#include <metall/detail/mutex.hpp>
#endif

#ifndef METALL_DISABLE_OBJECT_CACHE
#include <metall/kernel/object_cache.hpp>
#endif

namespace metall {
namespace kernel {

namespace {
namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;
}  // namespace

template <typename _chunk_no_type, typename _size_type,
          typename _difference_type, std::size_t _chunk_size,
          std::size_t _max_size, typename _segment_storage_type>
class segment_allocator {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using chunk_no_type = _chunk_no_type;
  using size_type = _size_type;
  using difference_type = _difference_type;
  static constexpr size_type k_chunk_size = _chunk_size;
  static constexpr size_type k_max_size = _max_size;
  static constexpr difference_type k_null_offset =
      std::numeric_limits<difference_type>::max();
  using segment_storage_type = _segment_storage_type;

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //
  static_assert(k_max_size < std::numeric_limits<size_type>::max(),
                "Max allocation size is too big");

  using myself =
      segment_allocator<_chunk_no_type, size_type, difference_type, _chunk_size,
                        _max_size, _segment_storage_type>;

  // For bin
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  using bin_no_type = typename bin_no_mngr::bin_no_type;
  static constexpr size_type k_num_small_bins = bin_no_mngr::num_small_bins();

  // For non-full chunk number bin (used to called 'bin directory')
  // NOTE: we only manage the non-full chunk numbers of the small bins (small
  // object sizes)
  using non_full_chunk_bin_type =
      bin_directory<k_num_small_bins, chunk_no_type>;
  static constexpr const char *k_non_full_chunk_bin_file_name =
      "non_full_chunk_bin";

  // For chunk directory
  using chunk_directory_type =
      chunk_directory<chunk_no_type, k_chunk_size, k_max_size>;
  using chunk_slot_no_type = typename chunk_directory_type::slot_no_type;
  static constexpr const char *k_chunk_directory_file_name = "chunk_directory";

  // For object cache
#ifndef METALL_DISABLE_OBJECT_CACHE
  using small_object_cache_type =
      object_cache<size_type, difference_type, bin_no_mngr, myself>;
#endif

#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
  using mutex_type = mdtl::mutex;
  using lock_guard_type = mdtl::mutex_lock_guard;
#endif

  // Threshold to enable the many allocation feature internally
  static constexpr std::size_t k_many_allocations_threshold = 4;

 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  explicit segment_allocator(segment_storage_type *segment_storage)
      : m_non_full_chunk_bin(),
        m_chunk_directory(k_max_size / k_chunk_size),
        m_segment_storage(segment_storage)
#ifndef METALL_DISABLE_OBJECT_CACHE
        ,
        m_object_cache()
#endif
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
        ,
        m_chunk_mutex(nullptr),
        m_bin_mutex(nullptr)
#endif
  {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    m_chunk_mutex = std::make_unique<mutex_type>();
    m_bin_mutex = std::make_unique<std::array<mutex_type, k_num_small_bins>>();
#endif
  }

  ~segment_allocator() noexcept = default;

  segment_allocator(const segment_allocator &) = delete;
  segment_allocator &operator=(const segment_allocator &) = delete;

  segment_allocator(segment_allocator &&) noexcept = default;
  segment_allocator &operator=(segment_allocator &&) noexcept = default;

 public:
  // -------------------- //
  // Public methods
  // -------------------- //

  /// \brief Allocates memory space
  /// \param nbytes
  /// \return The offset of an allocated memory.
  /// On error, k_null_offset is returned.
  difference_type allocate(const size_type nbytes) {
    if (nbytes == 0) return k_null_offset;
    const bin_no_type bin_no = bin_no_mngr::to_bin_no(nbytes);

    const auto offset = (priv_small_object_bin(bin_no))
                            ? priv_allocate_small_object(bin_no)
                            : priv_allocate_large_object(bin_no);
    assert(offset >= 0 || offset == k_null_offset);

    return offset;
  }

  /// \brief Allocate nbytes bytes of uninitialized storage whose alignment is
  /// specified by alignment. Note that this function adjusts an alignment only
  /// within this segment, i.e., this function does not know the address this
  /// segment is mapped to. \param nbytes A size to allocate. Must be a multiple
  /// of alignment. \param alignment An alignment requirement. Alignment must be
  /// a power of two and satisfy [min allocation size, system page size].
  /// \return On success, returns the pointer to the beginning of newly
  /// allocated memory. Returns k_null_offset, if the given arguments do not
  /// satisfy the requirements above.
  difference_type allocate_aligned(const size_type nbytes,
                                   const size_type alignment) {
    // This aligned allocation algorithm assumes that all power of 2 numbers
    // from the minimum allocation size (i.e., 8 bytes) to maximum allocation
    // size exist in the object size table

    // alignment must be equal to or larger than the min allocation size
    if (alignment < bin_no_mngr::to_object_size(0)) {
      return k_null_offset;
    }

    // alignment must be a power of 2
    if ((alignment != 0) && ((alignment & (alignment - 1)) != 0)) {
      return k_null_offset;
    }

    // This requirement could be removed, but it would need some work to do
    if (alignment > k_chunk_size) {
      return k_null_offset;
    }

    // nbytes must be a multiple of alignment
    if (nbytes % alignment != 0) {
      return k_null_offset;
    }

    // Internal allocation size must be a multiple of alignment
    assert(bin_no_mngr::to_object_size(bin_no_mngr::to_bin_no(nbytes)) %
               alignment == 0);

    // As long as the above requirements are satisfied, just calling the normal
    // allocate function is enough
    const auto offset = allocate(nbytes);
    assert(offset % alignment == 0 || offset == k_null_offset);

    return offset;
  }

  /// \brief Deallocates
  /// \param offset
  void deallocate(const difference_type offset) {
    if (offset == k_null_offset) return;
    assert(offset >= 0);

    const chunk_no_type chunk_no = offset / k_chunk_size;
    const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);

    if (priv_small_object_bin(bin_no)) {
      priv_deallocate_small_object(offset, bin_no);
    } else {
      priv_deallocate_large_object(chunk_no, bin_no);
    }
  }

  /// \brief Checks if all memory is deallocated.
  /// This function is not cheap if many objects are allocated.
  /// \return Returns true if all memory is deallocated.
  bool all_memory_deallocated() const {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type chunk_guard(*m_chunk_mutex);
#endif

    if (m_chunk_directory.size() == 0) {
      return true;
    }

#ifndef METALL_DISABLE_OBJECT_CACHE
    if (priv_check_all_small_allocations_are_cached_without_lock() &&
        m_chunk_directory.num_used_large_chunks() == 0) {
      return true;
    }
#endif

    return false;
  }

  /// \brief Returns the size of the segment being used.
  /// \return The size of the segment being used.
  /// \warning Be careful: the returned value can be incorrect because another
  /// thread can increase or decrease chunk directory size at the same time.
  size_type size() const { return m_chunk_directory.size() * k_chunk_size; }

  /// \brief
  /// \param base_path
  /// \return
  bool serialize(const fs::path &base_path) {
#ifndef METALL_DISABLE_OBJECT_CACHE
    priv_clear_object_cache();
#endif

    if (!m_non_full_chunk_bin.serialize(
            priv_make_file_name(base_path, k_non_full_chunk_bin_file_name))) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to serialize bin directory");
      return false;
    }
    if (!m_chunk_directory.serialize(
            priv_make_file_name(base_path, k_chunk_directory_file_name))) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to serialize chunk directory");
      return false;
    }
    return true;
  }

  /// \brief
  /// \param base_path
  /// \return
  bool deserialize(const fs::path &base_path) {
    if (!m_non_full_chunk_bin.deserialize(
            priv_make_file_name(base_path, k_non_full_chunk_bin_file_name))) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to deserialize bin directory");
      return false;
    }
    if (!m_chunk_directory.deserialize(
            priv_make_file_name(base_path, k_chunk_directory_file_name))) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to deserialize chunk directory");
      return false;
    }
    return true;
  }

  /// \brief
  /// \tparam out_stream_type
  /// \param log_out
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) {
#ifndef METALL_DISABLE_OBJECT_CACHE
    priv_clear_object_cache();
#endif

    std::vector<size_type> num_used_chunks_per_bin(bin_no_mngr::num_bins(), 0);

    (*log_out) << std::fixed;
    (*log_out) << std::setprecision(2);

    (*log_out) << "\nChunk Information"
               << "\n";
    (*log_out) << "[chunk no]\t[obj size (0 is empty)]\t[occupancy rate (%)]"
               << "\n";
    for (chunk_no_type chunk_no = 0; chunk_no < m_chunk_directory.size();
         ++chunk_no) {
      if (m_chunk_directory.unused_chunk(chunk_no)) {
        (*log_out) << chunk_no << "\t0\t0\n";
      } else {
        const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);
        ++num_used_chunks_per_bin[bin_no];

        const size_type object_size = bin_no_mngr::to_object_size(bin_no);

        if (bin_no < k_num_small_bins) {
          const size_type num_slots = m_chunk_directory.slots(chunk_no);
          const size_type num_occupied_slots =
              m_chunk_directory.occupied_slots(chunk_no);
          (*log_out) << chunk_no << "\t" << object_size << "\t"
                     << static_cast<double>(num_occupied_slots) / num_slots *
                            100
                     << "\n";
        } else {
          (*log_out) << chunk_no << "\t" << object_size << "\t100.0\n";
        }
      }
    }

    (*log_out) << "\nThe distribution of the sizes of being used chunks\n";
    (*log_out) << "(the number of used chunks at each object size)\n";
    (*log_out) << "[bin no]\t[obj size]\t[#of chunks (both full and non-full "
                  "chunks)]\n";
    for (size_type bin_no = 0; bin_no < num_used_chunks_per_bin.size();
         ++bin_no) {
      (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no)
                 << "\t" << num_used_chunks_per_bin[bin_no] << "\n";
    }

    (*log_out) << "\nThe distribution of the sizes of non-full chunks\n";
    (*log_out) << "NOTE: only chunks used for small objects are in the bin "
                  "directory\n";
    (*log_out) << "[bin no]\t[obj size]\t[#of non-full chunks]"
               << "\n";
    for (size_type bin_no = 0; bin_no < bin_no_mngr::num_small_bins();
         ++bin_no) {
      size_type num_non_full_chunks = std::distance(
          m_non_full_chunk_bin.begin(bin_no), m_non_full_chunk_bin.end(bin_no));
      (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no)
                 << "\t" << num_non_full_chunks << "\n";
    }
  }

 private:
  // -------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------- //
  bool priv_small_object_bin(const bin_no_type bin_no) const {
    return bin_no < k_num_small_bins;
  }

  fs::path priv_make_file_name(const fs::path &base_name,
                               const std::string &item_name) {
    return base_name.string() + "_" + item_name;
  }

  // ---------- For allocation ---------- //
  difference_type priv_allocate_small_object(const bin_no_type bin_no) {
#ifndef METALL_DISABLE_OBJECT_CACHE
    if (bin_no <= m_object_cache.max_bin_no()) {
      const auto offset = m_object_cache.pop(
          bin_no, this, &myself::priv_allocate_small_objects_from_global,
          &myself::priv_deallocate_small_objects_from_global);
      assert(offset >= 0 || offset == k_null_offset);
      return offset;
    }
#endif
    difference_type offset;
    priv_allocate_small_objects_from_global(bin_no, 1, &offset);
    return offset;
  }

  void priv_allocate_small_objects_from_global(
      const bin_no_type bin_no, const size_type num_allocates,
      difference_type *const allocated_offsets) {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type bin_guard(m_bin_mutex->at(bin_no));
#endif

    if (num_allocates >= k_many_allocations_threshold) {
      priv_allocate_many_small_objects_from_global_without_bin_lock(
          bin_no, num_allocates, allocated_offsets);
    } else {
      for (size_type i = 0; i < num_allocates; ++i) {
        allocated_offsets[i] =
            priv_allocate_small_object_from_global_without_bin_lock(bin_no);
      }
    }
  }

  difference_type priv_allocate_small_object_from_global_without_bin_lock(
      const bin_no_type bin_no) {
    const size_type object_size = bin_no_mngr::to_object_size(bin_no);

    if (m_non_full_chunk_bin.empty(bin_no) &&
        !priv_insert_new_small_object_chunk(bin_no)) {
      return k_null_offset;
    }

    assert(!m_non_full_chunk_bin.empty(bin_no));
    const chunk_no_type chunk_no = m_non_full_chunk_bin.front(bin_no);

    assert(!m_chunk_directory.all_slots_marked(chunk_no));
    const chunk_slot_no_type chunk_slot_no =
        m_chunk_directory.find_and_mark_slot(chunk_no);

    if (m_chunk_directory.all_slots_marked(chunk_no)) {
      m_non_full_chunk_bin.pop(bin_no);
    }
    const difference_type offset =
        k_chunk_size * chunk_no + object_size * chunk_slot_no;

    return offset;
  }

  void priv_allocate_many_small_objects_from_global_without_bin_lock(
      const bin_no_type bin_no, const size_type num_requested_allocates,
      difference_type *const allocated_offsets) {
    if (num_requested_allocates == 0) return;  // Not error, just no work.
    if (!allocated_offsets) return;

    std::fill_n(allocated_offsets, num_requested_allocates, k_null_offset);

    std::size_t cnt_allocations = 0;
    while (cnt_allocations < num_requested_allocates) {
      if (m_non_full_chunk_bin.empty(bin_no) &&
          !priv_insert_new_small_object_chunk(bin_no)) {
        return;
      }

      assert(!m_non_full_chunk_bin.empty(bin_no));
      const chunk_no_type chunk_no = m_non_full_chunk_bin.front(bin_no);
      assert(!m_chunk_directory.all_slots_marked(chunk_no));

      const std::size_t num_to_allocate =
          num_requested_allocates - cnt_allocations;
      // chunk_slot_no_type slots[num_to_allocate];
      auto slots = std::make_unique<chunk_slot_no_type[]>(num_to_allocate);
      const auto num_found_slots = m_chunk_directory.find_and_mark_many_slots(
          chunk_no, num_to_allocate, slots.get());
      assert(num_found_slots <= num_to_allocate);
      if (num_found_slots == 0) {
        break;
      }

      if (m_chunk_directory.all_slots_marked(chunk_no)) {
        m_non_full_chunk_bin.pop(bin_no);
      }

      const size_type object_size = bin_no_mngr::to_object_size(bin_no);
      for (std::size_t i = 0; i < num_found_slots; ++i) {
        allocated_offsets[cnt_allocations] =
            k_chunk_size * chunk_no + object_size * slots[i];
        ++cnt_allocations;
      }
    }
    assert(cnt_allocations == num_requested_allocates);
  }

  bool priv_insert_new_small_object_chunk(const bin_no_type bin_no) {
    chunk_no_type new_chunk_no;
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type chunk_guard(*m_chunk_mutex);
#endif
    new_chunk_no = m_chunk_directory.insert(bin_no);
    if (!priv_extend_segment_without_lock(new_chunk_no, 1)) {
      return false;
    }
    m_non_full_chunk_bin.insert(bin_no, new_chunk_no);
    return true;
  }

  difference_type priv_allocate_large_object(const bin_no_type bin_no) {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type chunk_guard(*m_chunk_mutex);
#endif
    const chunk_no_type new_chunk_no = m_chunk_directory.insert(bin_no);
    const size_type num_chunks =
        (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1) / k_chunk_size;
    if (!priv_extend_segment_without_lock(new_chunk_no, num_chunks)) {
      // Failed to extend the segment (fatal error)
      // Do clean up just in case and return k_null_offset
      m_chunk_directory.erase(new_chunk_no);
      return k_null_offset;
    }
    const difference_type offset = k_chunk_size * new_chunk_no;
    return offset;
  }

  bool priv_extend_segment_without_lock(const chunk_no_type head_chunk_no,
                                        const size_type num_chunks) {
    const size_type required_segment_size =
        (head_chunk_no + num_chunks) * k_chunk_size;
    if (required_segment_size <= m_segment_storage->size()) {
      return true;  // Has an enough segment size already
    }

    if (!m_segment_storage->extend(required_segment_size)) {
      std::stringstream ss;
      ss << "Failed to extend the segment to " << required_segment_size
         << " bytes";
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    return true;
  }

  // ---------- For deallocation ---------- //
  void priv_deallocate_small_object(const difference_type offset,
                                    const bin_no_type bin_no) {
#ifndef METALL_DISABLE_OBJECT_CACHE
    if (bin_no <= m_object_cache.max_bin_no()) {
      [[maybe_unused]] const bool ret = m_object_cache.push(
          bin_no, offset, this,
          &myself::priv_deallocate_small_objects_from_global);
      assert(ret);
      return;
    }
#endif
    priv_deallocate_small_objects_from_global(bin_no, 1, &offset);
  }

  void priv_deallocate_small_objects_from_global(
      const bin_no_type bin_no, const size_type num_deallocates,
      const difference_type offsets[]) {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type bin_guard(m_bin_mutex->at(bin_no));
#endif
    for (size_type i = 0; i < num_deallocates; ++i) {
      priv_deallocate_small_object_from_global_without_bin_lock(offsets[i],
                                                                bin_no);
    }
  }

  void priv_deallocate_small_object_from_global_without_bin_lock(
      const difference_type offset, const bin_no_type bin_no) {
    if (offset == k_null_offset) return;

    const size_type object_size = bin_no_mngr::to_object_size(bin_no);
    const chunk_no_type chunk_no = offset / k_chunk_size;
    const auto slot_no =
        static_cast<chunk_slot_no_type>((offset % k_chunk_size) / object_size);
    const bool was_full = m_chunk_directory.all_slots_marked(chunk_no);
    m_chunk_directory.unmark_slot(chunk_no, slot_no);
    if (was_full) {
      m_non_full_chunk_bin.insert(bin_no, chunk_no);
    } else if (m_chunk_directory.all_slots_unmarked(chunk_no)) {
      // All slots in the chunk are not used, deallocate it
      {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
        lock_guard_type chunk_guard(*m_chunk_mutex);
#endif
        m_chunk_directory.erase(chunk_no);
        priv_free_chunk(chunk_no, 1);
      }
      m_non_full_chunk_bin.erase(bin_no, chunk_no);

      return;
    }

#ifdef METALL_FREE_SMALL_OBJECT_SIZE_HINT
    priv_free_slot_without_bin_lock(object_size, chunk_no, slot_no,
                                    METALL_FREE_SMALL_OBJECT_SIZE_HINT);
#endif
  }

  void priv_free_slot_without_bin_lock(const size_type object_size,
                                       const chunk_no_type chunk_no,
                                       const chunk_slot_no_type slot_no,
                                       const size_type min_free_size_hint) {
    // To simplify the implementation, free slots only when object_size is at
    // least double of the page size
    const size_type min_free_size = std::max(
        (size_type)m_segment_storage->page_size() * 2, min_free_size_hint);
    if (object_size < min_free_size) return;

    // This function assumes that small objects are equal to or smaller than the
    // half chunk size
    assert(object_size <= k_chunk_size / 2);

    difference_type range_begin =
        chunk_no * k_chunk_size + slot_no * object_size;

    // Adjust the beginning of the range to free if it is not page aligned
    if (range_begin % m_segment_storage->page_size() != 0) {
      assert(slot_no > 0);  // Assume that chunk is page aligned

      if (m_chunk_directory.marked_slot(chunk_no, slot_no - 1)) {
        // Round up to the next multiple of page size
        // The left region will be freed when the previous slot is freed
        range_begin =
            mdtl::round_up(range_begin, m_segment_storage->page_size());
      } else {
        // The previous slot is not used, so round down the range_begin to align
        // it with the page size
        range_begin =
            mdtl::round_down(range_begin, m_segment_storage->page_size());
      }
    }
    assert(range_begin % m_segment_storage->page_size() == 0);
    assert(range_begin / k_chunk_size == chunk_no);

    difference_type range_end =
        chunk_no * k_chunk_size + slot_no * object_size + object_size;
    // Adjust the end of the range to free if it is not page aligned
    // Use the same logic as range_begin
    if (range_end % m_segment_storage->page_size() != 0) {
      // If this is the last slot of the chunk, the end position must be page
      // aligned
      assert(object_size * (slot_no + 1) < k_chunk_size);

      if (m_chunk_directory.marked_slot(chunk_no, slot_no + 1)) {
        range_end = mdtl::round_down(range_end, m_segment_storage->page_size());
      } else {
        range_end = mdtl::round_up(range_end, m_segment_storage->page_size());
      }
    }
    assert(range_end % m_segment_storage->page_size() == 0);
    assert((range_end - 1) / k_chunk_size == chunk_no);

    assert(range_begin < range_end);
    const size_type free_size = range_end - range_begin;
    assert(free_size % m_segment_storage->page_size() == 0);

    m_segment_storage->free_region(range_begin, free_size);
  }

  void priv_deallocate_large_object(const chunk_no_type chunk_no,
                                    const bin_no_type bin_no) {
#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
    lock_guard_type chunk_guard(*m_chunk_mutex);
#endif
    m_chunk_directory.erase(chunk_no);
    const size_type num_chunks =
        (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1) / k_chunk_size;
    priv_free_chunk(chunk_no, num_chunks);
  }

  void priv_free_chunk(const chunk_no_type head_chunk_no,
                       const size_type num_chunks) {
    const off_t offset = head_chunk_no * k_chunk_size;
    const size_type length = num_chunks * k_chunk_size;
    assert(offset + length <= m_segment_storage->size());
    m_segment_storage->free_region(offset, length);
  }

  // ---------- For object cache ---------- //
#ifndef METALL_DISABLE_OBJECT_CACHE
  void priv_clear_object_cache() {
    m_object_cache.clear(this,
                         &myself::priv_deallocate_small_objects_from_global);
  }
#endif

#ifndef METALL_DISABLE_OBJECT_CACHE
  /// \brief Checks if all marked (used) slots in the chunk directory exist in
  /// the object cache.
  bool priv_check_all_small_allocations_are_cached_without_lock() const {
    const auto marked_slots = m_chunk_directory.get_all_marked_slots();
    std::set<difference_type> small_allocs;
    for (const auto &item : marked_slots) {
      const auto chunk_no = std::get<0>(item);
      const auto bin_no = std::get<1>(item);
      const auto slot_no = std::get<2>(item);

      const size_type object_size = bin_no_mngr::to_object_size(bin_no);
      small_allocs.insert(k_chunk_size * chunk_no + object_size * slot_no);
    }

    for (unsigned int c = 0; c < m_object_cache.num_caches(); ++c) {
      for (bin_no_type b = 0; b <= m_object_cache.max_bin_no(); ++b) {
        for (auto itr = m_object_cache.begin(c, b),
                  end = m_object_cache.end(c, b);
             itr != end; ++itr) {
          const auto offset = *itr;
          if (small_allocs.count(offset) == 0) {
            return false;
          }
          small_allocs.erase(offset);
        }
      }
    }

    return small_allocs.empty();
  }
#endif

  // -------------------- //
  // Private fields
  // -------------------- //
  non_full_chunk_bin_type m_non_full_chunk_bin;
  chunk_directory_type m_chunk_directory;
  segment_storage_type *m_segment_storage{nullptr};

#ifndef METALL_DISABLE_OBJECT_CACHE
  small_object_cache_type m_object_cache;
#endif

#ifdef METALL_ENABLE_MUTEX_IN_SEGMENT_ALLOCATOR
  std::unique_ptr<mutex_type> m_chunk_mutex{nullptr};
  std::unique_ptr<std::array<mutex_type, k_num_small_bins>> m_bin_mutex{
      nullptr};
#endif
};

}  // namespace kernel
}  // namespace metall
#endif  // METALL_KERNEL_SEGMENT_ALLOCATOR_HPP
