// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_V0_KERNEL_SEGMENT_ALLOCATOR_HPP
#define METALL_V0_KERNEL_SEGMENT_ALLOCATOR_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <iomanip>

#include <metall/v0/kernel/bin_number_manager.hpp>
#include <metall/v0/kernel/bin_directory.hpp>
#include <metall/v0/kernel/chunk_directory.hpp>
#include <metall/v0/kernel/object_size_manager.hpp>
#include <metall/detail/utility/char_ptr_holder.hpp>

#define ENABLE_MUTEX_IN_V0_MANAGER_KERNEL 1
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
#include <metall/detail/utility/mutex.hpp>
#endif

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <typename _chunk_no_type, typename size_type, typename difference_type,
    std::size_t _chunk_size, std::size_t _max_size,
          typename _segment_storage_type,
          typename _internal_data_allocator_type>
class segment_allocator {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using chunk_no_type = _chunk_no_type;
  static constexpr std::size_t k_chunk_size = _chunk_size;
  static constexpr std::size_t k_max_size = _max_size;
  using segment_storage_type = _segment_storage_type;
  using internal_data_allocator_type = _internal_data_allocator_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  // For bin
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  using bin_no_type = typename bin_no_mngr::bin_no_type;
  static constexpr size_type k_num_small_bins = bin_no_mngr::num_small_bins();

  // For bin directory (NOTE: we only manage small bins)
  using bin_directory_type = bin_directory<k_num_small_bins, chunk_no_type, internal_data_allocator_type>;
  static constexpr const char *k_bin_directory_file_name = "bin_directory";

  // For chunk directory
  using chunk_directory_type = chunk_directory<chunk_no_type, k_chunk_size, k_max_size, internal_data_allocator_type>;
  using chunk_slot_no_type = typename chunk_directory_type::slot_no_type;
  static constexpr const char *k_chunk_directory_file_name = "chunk_directory";

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit segment_allocator(segment_storage_type *segment_storage,
                             const internal_data_allocator_type &allocator = internal_data_allocator_type())
      : m_bin_directory(allocator),
        m_chunk_directory(allocator),
        m_segment_storage(segment_storage)
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      , m_chunk_mutex(),
        m_bin_mutex()
#endif
  {
    m_chunk_directory.allocate(k_max_size / k_chunk_size); // TODO: make a function returns #chunks
  }

  ~segment_allocator() = default;

  segment_allocator(const segment_allocator &) = delete;
  segment_allocator &operator=(const segment_allocator &) = delete;

  segment_allocator(segment_allocator &&) = default;
  segment_allocator &operator=(segment_allocator &&) = default;

 public:
  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //

  /// \brief Allocates memory space
  /// \param nbytes
  /// \return
  difference_type allocate(const size_type nbytes) {
    const bin_no_type bin_no = bin_no_mngr::to_bin_no(nbytes);
    if (priv_small_object_bin(bin_no)) {
      return priv_allocate_small_object(bin_no);
    }
    return priv_allocate_large_object(bin_no);
  }

  // \TODO: implement
  difference_type allocate_aligned(const size_type nbytes, const size_type alignment) {
    assert(false);
    return nullptr;
  }

  /// \brief Deallocates
  /// \param offset
  void deallocate(const difference_type offset) {
    const chunk_no_type chunk_no = offset / k_chunk_size;
    const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);

    if (priv_small_object_bin(bin_no)) {
      priv_deallocate_small_object(offset, chunk_no, bin_no);
    } else {
      priv_deallocate_large_object(chunk_no, bin_no);
    }
  }

  /// \brief
  /// \return Returns the size of the segment range being used
  size_type size() const {
    return m_chunk_directory.size() * k_chunk_size;
  }

  bool serialize(const std::string &base_path) {
    if (!m_bin_directory.serialize(priv_make_file_name(base_path, k_bin_directory_file_name).c_str())) {
      std::cerr << "Failed to serialize bin directory" << std::endl;
      return false;
    }
    if (!m_chunk_directory.serialize(priv_make_file_name(base_path, k_chunk_directory_file_name).c_str())) {
      std::cerr << "Failed to serialize chunk directory" << std::endl;
      return false;
    }
    return true;
  }

  bool deserialize(const std::string &base_path) {
    if (!m_bin_directory.deserialize(priv_make_file_name(base_path, k_bin_directory_file_name).c_str())) {
      std::cerr << "Failed to deserialize bin directory" << std::endl;
      return false;
    }
    if (!m_chunk_directory.deserialize(priv_make_file_name(base_path, k_chunk_directory_file_name).c_str())) {
      std::cerr << "Failed to deserialize chunk directory" << std::endl;
      return false;
    }
    return true;
  }

  template <typename out_stream_type>
  void profile(out_stream_type *log_out) const {
    std::vector<std::size_t> num_used_chunks_per_bin(bin_no_mngr::num_bins(), 0);

    (*log_out) << std::fixed;
    (*log_out) << std::setprecision(2);

    (*log_out) << "\nChunk Information" << "\n";
    (*log_out) << "[chunk no]\t[obj size (0 is empty)]\t[occupancy rate (%)]" << "\n";
    for (chunk_no_type chunk_no = 0; chunk_no < m_chunk_directory.size(); ++chunk_no) {

      if (m_chunk_directory.empty_chunk(chunk_no)) {
        (*log_out) << chunk_no << "\t0\t0\n";
      } else {
        const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);
        ++num_used_chunks_per_bin[bin_no];

        const size_type object_size = bin_no_mngr::to_object_size(bin_no);

        if (bin_no < k_num_small_bins) {
          const std::size_t num_slots = m_chunk_directory.slots(chunk_no);
          const std::size_t num_occupied_slots = m_chunk_directory.occupied_slots(chunk_no);
          (*log_out) << chunk_no << "\t" << object_size
                     << "\t" << static_cast<double>(num_occupied_slots) / num_slots * 100 << "\n";
        } else {
          (*log_out) << chunk_no << "\t" << object_size << "\t100.0\n";
        }
      }
    }

    (*log_out) << "\nThe distribution of the sizes of being used chunks\n";
    (*log_out) << "(the number of used chunks at each object size)\n";
    (*log_out) << "[bin no]\t[obj size]\t[#of chunks (both full and non-full chunks)]\n";
    for (std::size_t bin_no = 0; bin_no < num_used_chunks_per_bin.size(); ++bin_no) {
      (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no)
                 << "\t" << num_used_chunks_per_bin[bin_no] << "\n";
    }

    (*log_out) << "\nThe distribution of the sizes of non-full chunks\n";
    (*log_out) << "NOTE: only chunks used for small objects are in the bin directory\n";
    (*log_out) << "[bin no]\t[obj size]\t[#of non-full chunks]" << "\n";
    for (std::size_t bin_no = 0; bin_no < bin_no_mngr::num_small_bins(); ++bin_no) {
      std::size_t num_non_full_chunks = std::distance(m_bin_directory.begin(bin_no), m_bin_directory.end(bin_no));
      (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no) << "\t" << num_non_full_chunks << "\n";
    }
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  bool priv_small_object_bin(const bin_no_type bin_no) const {
    return bin_no < k_num_small_bins;
  }

  std::string priv_make_file_name(const std::string &base_name, const std::string &item_name) {
    return base_name + "_" + item_name;
  }

  // ---------------------------------------- For allocation ---------------------------------------- //
  difference_type priv_allocate_small_object(const bin_no_type bin_no) {
    const size_type object_size = bin_no_mngr::to_object_size(bin_no);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type bin_guard(m_bin_mutex[bin_no]);
#endif

    if (m_bin_directory.empty(bin_no)) {
      chunk_no_type new_chunk_no;
      {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
        lock_guard_type chunk_guard(m_chunk_mutex);
#endif
        new_chunk_no = m_chunk_directory.insert(bin_no);
      }
      m_bin_directory.insert(bin_no, new_chunk_no);
      priv_extend_segment(new_chunk_no, 1);
    }

    assert(!m_bin_directory.empty(bin_no));
    const chunk_no_type chunk_no = m_bin_directory.front(bin_no);

    assert(!m_chunk_directory.all_slots_marked(chunk_no));
    const chunk_slot_no_type chunk_slot_no = m_chunk_directory.find_and_mark_slot(chunk_no);

    if (m_chunk_directory.all_slots_marked(chunk_no)) {
      m_bin_directory.pop(bin_no);
    }

    const difference_type offset = k_chunk_size * chunk_no + object_size * chunk_slot_no;
    return offset;
  }

  difference_type priv_allocate_large_object(const bin_no_type bin_no) {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type chunk_guard(m_chunk_mutex);
#endif
    const chunk_no_type new_chunk_no = m_chunk_directory.insert(bin_no);
    const std::size_t num_chunks = (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1) / k_chunk_size;
    priv_extend_segment(new_chunk_no, num_chunks);
    const difference_type offset = k_chunk_size * new_chunk_no;
    return offset;
  }

  void priv_extend_segment(const chunk_no_type head_chunk_no, const size_type num_chunks) {
    // TODO: implement more sophisticated algorithm
    const size_type required_segment_size = (head_chunk_no + num_chunks) * k_chunk_size;
    if (required_segment_size <= m_segment_storage->size()) {
      return;
    }
    const auto size = std::max((size_type)required_segment_size, (size_type)(m_segment_storage->size() * 2));
    if (!m_segment_storage->extend(size)) {
      std::cerr << "Failed to extend application data segment to " << size << " bytes" << std::endl;
      std::abort();
    }
  }

  // ---------------------------------------- For deallocation ---------------------------------------- //
  void priv_deallocate_small_object(const difference_type offset,
                                    const chunk_no_type chunk_no,
                                    const bin_no_type bin_no) {
    const size_type object_size = bin_no_mngr::to_object_size(bin_no);
    const auto slot_no = static_cast<chunk_slot_no_type>((offset % k_chunk_size) / object_size);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type bin_guard(m_bin_mutex[bin_no]);
#endif

    const bool was_full = m_chunk_directory.all_slots_marked(chunk_no);
    m_chunk_directory.unmark_slot(chunk_no, slot_no);

    if (was_full) {
      m_bin_directory.insert(bin_no, chunk_no);
    } else if (m_chunk_directory.all_slots_unmarked(chunk_no)) {
      // All slots in the chunk are not used, deallocate it
      {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
        lock_guard_type chunk_guard(m_chunk_mutex);
#endif
        m_chunk_directory.erase(chunk_no);
        priv_free_chunk(chunk_no, 1);
      }
      m_bin_directory.erase(bin_no, chunk_no);

      return;
    }

#ifdef METALL_FREE_SMALL_OBJECT_SIZE_HINT
    priv_free_slot(object_size, chunk_no, slot_no, METALL_FREE_SMALL_OBJECT_SIZE_HINT);
#endif
  }

  void priv_free_slot(const size_type object_size,
                      const chunk_no_type chunk_no,
                      const chunk_slot_no_type slot_no,
                      const size_type min_free_size_hint) {

    // To simplify the implementation, free slots only when object_size is at least double of the page size
    const size_type
        min_free_size = std::max((size_type)m_segment_storage->page_size() * 2, (size_type)min_free_size_hint);
    if (object_size < min_free_size) return;

    // This function assumes that small objects are equal to or smaller than the half chunk size
    assert(object_size <= k_chunk_size / 2);

    difference_type range_begin = chunk_no * k_chunk_size + slot_no * object_size;

    // Adjust the beginning of the range to free if it is not page aligned
    if (range_begin % m_segment_storage->page_size() != 0) {
      assert(slot_no > 0); // Assume that chunk is page aligned

      if (m_chunk_directory.slot_marked(chunk_no, slot_no - 1)) {
        // Round up to the next multiple of page size
        // The left region will be freed when the previous slot is freed
        range_begin = util::round_up(range_begin, m_segment_storage->page_size());
      } else {
        // The previous slot is not used, so round down the range_begin to align it with the page size
        range_begin = util::round_down(range_begin, m_segment_storage->page_size());
      }
    }
    assert(range_begin % m_segment_storage->page_size() == 0);
    assert(range_begin / k_chunk_size == chunk_no);

    difference_type range_end = chunk_no * k_chunk_size + slot_no * object_size + object_size;
    // Adjust the end of the range to free if it is not page aligned
    // Use the same logic as range_begin
    if (range_end % m_segment_storage->page_size() != 0) {

      // If this is the last slot of the chunk, the end position must be page aligned
      assert(object_size * (slot_no + 1) < k_chunk_size);

      if (m_chunk_directory.slot_marked(chunk_no, slot_no + 1)) {
        range_end = util::round_down(range_end, m_segment_storage->page_size());
      } else {
        range_end = util::round_up(range_end, m_segment_storage->page_size());
      }
    }
    assert(range_end % m_segment_storage->page_size() == 0);
    assert((range_end - 1) / k_chunk_size == chunk_no);

    assert(range_begin < range_end);
    const size_type free_size = range_end - range_begin;
    assert(free_size % m_segment_storage->page_size() == 0);

    m_segment_storage->free_region(range_begin, free_size);
  }

  void priv_deallocate_large_object(const chunk_no_type chunk_no, const bin_no_type bin_no) {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type chunk_guard(m_chunk_mutex);
#endif
    m_chunk_directory.erase(chunk_no);
    const std::size_t num_chunks = (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1) / k_chunk_size;
    priv_free_chunk(chunk_no, num_chunks);
  }

  void priv_free_chunk(const chunk_no_type head_chunk_no, const size_type num_chunks) {
    const off_t offset = head_chunk_no * k_chunk_size;
    const size_type length = num_chunks * k_chunk_size;
    assert(offset + length <= m_segment_storage->size());
    m_segment_storage->free_region(offset, length);
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  bin_directory_type m_bin_directory;
  chunk_directory_type m_chunk_directory;
  segment_storage_type *m_segment_storage;

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  mutex_type m_chunk_mutex;
  std::array<mutex_type, k_num_small_bins> m_bin_mutex;
#endif
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_V0_KERNEL_SEGMENT_ALLOCATOR_HPP
