// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP
#define METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <vector>
#include <map>

#include <metall/offset_ptr.hpp>
#include <metall/v0/kernel/bin_number_manager.hpp>
#include <metall/v0/kernel/bin_directory.hpp>
#include <metall/v0/kernel/chunk_directory.hpp>
#include <metall/v0/kernel/named_object_directory.hpp>
#include <metall/v0/kernel/segment_storage.hpp>
#include <metall/v0/kernel/object_size_manager.hpp>
#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/in_place_interface.hpp>
#include <metall/detail/utility/array_construct.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/char_ptr_holder.hpp>
#include <metall/detail/utility/soft_dirty_page.hpp>

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

/// \brief Manager kernel class version 0
/// \tparam chunk_no_type Type of chunk number
/// \tparam k_chunk_size Size of single chunk in byte
/// \tparam allocator_type Allocator used to allocate internal data
template <typename chunk_no_type, std::size_t k_chunk_size, typename allocator_type>
class manager_kernel {

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using void_pointer = offset_ptr<void>;
  using char_type = char; // required by boost's named proxy
  using char_ptr_holder_type = util::char_ptr_holder<char_type>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using id_type = uint16_t;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = manager_kernel<chunk_no_type, k_chunk_size, allocator_type>;

  static constexpr size_type k_max_size = 1ULL << 48; // TODO: get from somewhere else

  // For bin
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  using bin_no_type = typename bin_no_mngr::bin_no_type;
  static constexpr size_type k_num_small_bins = bin_no_mngr::num_small_bins();
  static constexpr const char *k_segment_file_name = "segment";

  // For bin directory (NOTE: we only manage small bins)
  using bin_directory_type = bin_directory<k_num_small_bins, chunk_no_type, allocator_type>;
  static constexpr const char *k_bin_directory_file_name = "bin_directory";

  // For chunk directory
  using chunk_directory_type = chunk_directory<chunk_no_type, k_chunk_size, k_max_size, allocator_type>;
  using chunk_slot_no_type = typename chunk_directory_type::slot_no_type;
  static constexpr const char *k_chunk_directory_file_name = "chunk_directory";

  // For named object directory
  using named_object_directory_type = named_object_directory<difference_type, size_type, allocator_type>;
  static constexpr const char *k_named_object_directory_file_name = "named_object_directory";

  // For data segment manager
  using segment_storage_type = segment_storage<difference_type, size_type>;

  static constexpr size_type k_max_small_object_size = bin_no_mngr::to_object_size(bin_no_mngr::num_small_bins() - 1);

  // For snapshot
  static constexpr size_type k_snapshot_no_safeguard = 1ULL << 20ULL; // Safeguard, you could increase this number
  static constexpr const char *k_diff_snapshot_file_name_prefix = "ssdf";
  static constexpr size_type k_min_snapshot_no = 1;

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit manager_kernel(const allocator_type &allocator)
      : m_file_base_path(),
        m_bin_directory(allocator),
        m_chunk_directory(allocator),
        m_named_object_directory(allocator),
        m_segment_storage() {}

  ~manager_kernel() {
    close();
  }

  manager_kernel(const manager_kernel &) = delete;
  manager_kernel &operator=(const manager_kernel &) = delete;

  manager_kernel(manager_kernel &&) noexcept = default;
  manager_kernel &operator=(manager_kernel &&) noexcept = default;

 public:
  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Expect to be called by a single thread
  /// \param path
  /// \param nbytes
  void create(const char *path, const size_type nbytes) {
    m_file_base_path = path;

    if (!m_segment_storage.create(priv_make_file_name(k_segment_file_name).c_str(), nbytes)) {
      std::cerr << "Can not create segment" << std::endl;
      std::abort();
    }

    m_chunk_directory.reserve(priv_num_chunks());
  }

  /// \brief Expect to be called by a single thread
  /// \param path
  /// \return
  bool open(const char *path) {
    m_file_base_path = path;

    if (!m_segment_storage.open(priv_make_file_name(k_segment_file_name).c_str())) return false;

    m_chunk_directory.reserve(priv_num_chunks());

    const auto snapshot_no = priv_find_next_snapshot_no(path);
    if (snapshot_no == 0) {
      std::abort();
    } else if (snapshot_no == k_min_snapshot_no) { // Open normal files, i.e., not diff snapshot
      return deserialize_management_data(path);
    } else { // Open diff snapshot files
      const auto diff_pages_list = priv_merge_segment_diff_list(path, snapshot_no - 1);
      if (!priv_apply_segment_diff(path, snapshot_no - 1, diff_pages_list)) {
        std::cerr << "Cannot apply segment diff" << std::endl;
        std::abort();
      }
      return deserialize_management_data(priv_make_snapshot_base_file_name(path, snapshot_no - 1).c_str());
    }

    assert(false);
    return false;
  }

  /// \brief Expect to be called by a single thread
  void close() {
    if (priv_initialized()) {
      sync();
      m_segment_storage.destroy();
    }
  }

  /// \brief Sync with backing files
  void sync() {
    assert(priv_initialized());

    m_segment_storage.sync();

    if (!m_bin_directory.serialize(priv_make_file_name(k_bin_directory_file_name).c_str()) ||
        !m_chunk_directory.serialize(priv_make_file_name(k_chunk_directory_file_name).c_str()) ||
        !m_named_object_directory.serialize(priv_make_file_name(k_named_object_directory_file_name).c_str())) {
      std::cerr << "Failed to serialize internal data" << std::endl;
      std::abort();
    }
  }

  void *allocate(const size_type nbytes) {
    assert(priv_initialized());

    const bin_no_type bin_no = bin_no_mngr::to_bin_no(nbytes);

    if (bin_no < k_num_small_bins) {
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
      }

      assert(!m_bin_directory.empty(bin_no));
      const chunk_no_type chunk_no = m_bin_directory.front(bin_no);

      assert(!m_chunk_directory.full_slot(chunk_no));
      const chunk_slot_no_type chunk_slot_no = m_chunk_directory.find_and_mark_slot(chunk_no);

      if (m_chunk_directory.full_slot(chunk_no)) {
        m_bin_directory.pop(bin_no);
      }

      return static_cast<char *>(m_segment_storage.segment()) + k_chunk_size * chunk_no + object_size * chunk_slot_no;

    } else {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type chunk_guard(m_chunk_mutex);
#endif
      const chunk_no_type new_chunk_no = m_chunk_directory.insert(bin_no);
      return static_cast<char *>(m_segment_storage.segment()) + k_chunk_size * new_chunk_no;
    }

    return nullptr;
  }

  // \TODO: implement
  void *allocate_aligned(const size_type nbytes, const size_type alignment) {
    assert(false);
    return nullptr;
  }

  void deallocate(void *addr) {
    assert(priv_initialized());

    if (!addr) return;

    const difference_type offset = static_cast<char *>(addr) - static_cast<char *>(m_segment_storage.segment());
    const chunk_no_type chunk_no = offset / k_chunk_size;
    const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);

    if (bin_no < k_num_small_bins) {
      const size_type object_size = bin_no_mngr::to_object_size(bin_no);
      const auto slot_no = static_cast<chunk_slot_no_type>((offset % k_chunk_size) / object_size);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type bin_guard(m_bin_mutex[bin_no]);
#endif

      const bool was_full = m_chunk_directory.full_slot(chunk_no);
      m_chunk_directory.unmark_slot(chunk_no, slot_no);
      if (was_full) {
        m_bin_directory.insert(bin_no, chunk_no);
      } else if (m_chunk_directory.empty_slot(chunk_no)) {
        {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
          lock_guard_type chunk_guard(m_chunk_mutex);
#endif
          m_chunk_directory.erase(chunk_no);
          priv_free_chunk(chunk_no, 1);
        }
        m_bin_directory.erase(bin_no, chunk_no);
      }
    } else {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type chunk_guard(m_chunk_mutex);
#endif
      m_chunk_directory.erase(chunk_no);
      const std::size_t num_chunks = bin_no_mngr::to_object_size(bin_no) / k_chunk_size;
      priv_free_chunk(chunk_no, num_chunks);
    }
  }

  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      return std::make_pair(nullptr, 0);
    }

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type guard(m_named_object_directory_mutex); // TODO: don't need at here?
#endif

    const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();

    const auto iterator = m_named_object_directory.find(raw_name);
    if (iterator == m_named_object_directory.end()) {
      return std::make_pair<T *, size_type>(nullptr, 0);
    }

    const auto offset = std::get<1>(iterator->second);
    const auto length = std::get<2>(iterator->second);
    return std::make_pair(reinterpret_cast<T *>(offset + static_cast<char *>(m_segment_storage.segment())), length);
  }

  template <typename T>
  bool destroy(char_ptr_holder_type name) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      return false;
    }

    difference_type offset;
    size_type length;
    { // Erase from named_object_directory
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type guard(m_named_object_directory_mutex);
#endif

      const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();

      const auto iterator = m_named_object_directory.find(raw_name);
      if (iterator == m_named_object_directory.end()) return false; // No object with the name
      m_named_object_directory.erase(iterator);
      offset = std::get<1>(iterator->second);
      length = std::get<2>(iterator->second);
    }

    // Destruct each object
    auto ptr = static_cast<T *>(static_cast<void *>(offset + static_cast<char *>(m_segment_storage.segment())));
    for (size_type i = 0; i < length; ++i) {
      ptr->~T();
      ++ptr;
    }

    deallocate(offset + static_cast<char *>(m_segment_storage.segment()));

    return true;
  }

  /// \brief Generic named/anonymous new function. This method is required by construct_proxy and construct_iter_proxy
  /// \tparam T Type of the object(s)
  /// \param name Name of the object(s)
  /// \param num Number of objects to be constructed
  /// \param try2find If true, tries to find already constructed object(s) with the same name
  /// \param dothrow If true, throws exception
  /// \param table Reference to an in_place_interface object
  /// \return Returns a pointer to the constructed object(s)
  template <typename T>
  T *generic_construct(char_ptr_holder_type name,
                       const size_type num,
                       const bool try2find,
                       const bool dothrow,
                       util::in_place_interface &table) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      void *const ptr = allocate(num * sizeof(T));
      util::array_construct(ptr, num, table);
      return static_cast<T *>(ptr);
    } else {
      const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();
      return priv_generic_named_construct<T>(raw_name, num, try2find, dothrow, table);
    }
  }

  /// \brief
  /// \param base_path
  /// \return
  bool snapshot(const char *destination_base_path) {
    sync();
    return priv_snapshot_entire_data(destination_base_path);
  }

  /// \brief
  /// \param destination_base_path
  /// \return
  bool snapshot_diff(const char *destination_base_path) {
    sync();
    return priv_snapshot_diff_data(destination_base_path);
  }

  /// \brief Copies backing files synchronously
  /// \param source_base_path
  /// \param destination_base_path
  /// \return If succeeded, returns True; other false
  static bool copy_file(const char *source_base_path, const char *destination_base_path) {
    return priv_copy_all_backing_files(source_base_path, destination_base_path,
                                       priv_find_next_snapshot_no(source_base_path) - 1);
  }

  /// \brief Copies backing files asynchronously
  /// \param source_base_path
  /// \param destination_base_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> copy_file_async(const char *source_base_path, const char *destination_base_path) {
    return std::async(std::launch::async, copy_file, source_base_path, destination_base_path);
  }

  /// \brief Remove backing files synchronously
  /// \param base_path
  /// \return If succeeded, returns True; other false
  static bool remove_file(const char *base_path) {
    return priv_remove_all_backing_files(base_path, priv_find_next_snapshot_no(base_path) - 1);
  }

  /// \brief Remove backing files asynchronously
  /// \param base_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> remove_file_async(const char *base_path) {
    return std::async(std::launch::async, remove_file, base_path);
  }

  // Implemented in another file.
  void profile(const std::string &log_file_name) const;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  static std::string priv_make_file_name(const std::string &base_name, const std::string &item_name) {
    return base_name + "_" + item_name;
  }

  bool priv_initialized() const {
    return (m_segment_storage.segment() && m_segment_storage.size() && !m_file_base_path.empty());
  }

  std::string priv_make_file_name(const std::string &item_name) const {
    return priv_make_file_name(m_file_base_path, item_name);
  }

  void priv_free_chunk(const chunk_no_type head_chunk_no, const std::size_t num_chunks) {
    const off_t offset = head_chunk_no * k_chunk_size;
    const size_t length = num_chunks * k_chunk_size;
    m_segment_storage.free_region(offset, length);
  }

  std::size_t priv_num_chunks() const {
    return m_segment_storage.size() / k_chunk_size;
  }

  template <typename T>
  T *priv_generic_named_construct(const char_type *const name,
                                  const size_type num,
                                  const bool try2find,
                                  const bool, // TODO implement 'dothrow'
                                  util::in_place_interface &table) {
    void *ptr = nullptr;
    {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type guard(m_named_object_directory_mutex); // TODO: implement a better lock strategy
#endif

      const auto iterator = m_named_object_directory.find(name);
      if (iterator != m_named_object_directory.end()) { // Found an entry
        if (try2find) {
          const auto offset = std::get<1>(iterator->second);
          return reinterpret_cast<T *>(offset + static_cast<char *>(m_segment_storage.segment()));
        } else {
          return nullptr;
        }
      }

      ptr = allocate(num * sizeof(T));

      // Insert into named_object_directory
      const auto insert_ret = m_named_object_directory.insert(name,
                                                              static_cast<char *>(ptr)
                                                                  - static_cast<char *>(m_segment_storage.segment()),
                                                              num);
      if (!insert_ret) {
        std::cerr << "Failed to insert a new name: " << name << std::endl;
        return nullptr;
      }
    }

    util::array_construct(ptr, num, table);

    return static_cast<T *>(ptr);
  }

  bool deserialize_management_data(const char *const base_path) {
    if (!m_bin_directory.deserialize(priv_make_file_name(base_path, k_bin_directory_file_name).c_str()) ||
        !m_chunk_directory.deserialize(priv_make_file_name(base_path, k_chunk_directory_file_name).c_str()) ||
        !m_named_object_directory.deserialize(priv_make_file_name(base_path,
                                                                  k_named_object_directory_file_name).c_str())) {
      std::cerr << "Cannot deserialize internal data" << std::endl;
      return false;
    }
    return true;
  }

  // ---------------------------------------- File operations ---------------------------------------- //
  static bool priv_copy_backing_file(const char *const src_base_name,
                                     const char *const dst_base_name,
                                     const char *const item_name) {
    if (!util::copy_file(priv_make_file_name(src_base_name, item_name),
                         priv_make_file_name(dst_base_name, item_name))) {
//      std::cerr << "Failed to copy a backing file: " << priv_make_file_name(dst_base_name, item_name)
//                << std::endl;
      return false;
    }
    return true;
  }

  /// \brief Copies all backing files
  static bool priv_copy_all_backing_files(const char *const src_base_name,
                                          const char *const dst_base_name) {
    bool ret = true;
    ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_segment_file_name);
    ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_bin_directory_file_name);
    ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_chunk_directory_file_name);
    ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_named_object_directory_file_name);

    return ret;
  }

  /// \brief Copies all backing files including snapshots
  static bool priv_copy_all_backing_files(const char *const src_base_name,
                                          const char *const dst_base_name,
                                          const size_type max_snapshot_no) {
    bool ret = true;

    ret &= priv_copy_all_backing_files(src_base_name, dst_base_name);

    for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
      const std::string src_base_file_name = priv_make_snapshot_base_file_name(src_base_name, snapshop_no);
      const std::string dst_base_file_name = priv_make_snapshot_base_file_name(dst_base_name, snapshop_no);
      ret &= priv_copy_all_backing_files(src_base_file_name.c_str(), dst_base_file_name.c_str());
    }

    return ret;
  }

  static bool priv_remove_backing_file(const char *const base_name, const char *const item_name) {
    if (!util::remove_file(priv_make_file_name(base_name, item_name))) {
//      std::cerr << "Failed to remove a backing file: "
//                << priv_make_file_name(base_name, item_name) << std::endl;
      return false;
    }
    return true;
  }

  /// \brief Removes all backing files
  static bool priv_remove_all_backing_files(const char *const base_name) {
    bool ret = true;

    ret &= priv_remove_backing_file(base_name, k_segment_file_name);
    ret &= priv_remove_backing_file(base_name, k_chunk_directory_file_name);
    ret &= priv_remove_backing_file(base_name, k_bin_directory_file_name);
    ret &= priv_remove_backing_file(base_name, k_named_object_directory_file_name);

    return ret;
  }

  /// \brief Removes all backing files including snapshots
  static bool priv_remove_all_backing_files(const char *const base_name,
                                            const size_type max_snapshot_no) {
    bool ret = true;

    ret &= priv_remove_all_backing_files(base_name);

    for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
      const std::string base_file_name = priv_make_snapshot_base_file_name(base_name, snapshop_no);
      ret &= priv_remove_all_backing_files(base_file_name.c_str());
    }

    return ret;
  }

  // ---------------------------------------- Snapshot ---------------------------------------- //
  bool priv_snapshot_entire_data(const char *snapshot_base_path) const {
    return priv_copy_all_backing_files(m_file_base_path.c_str(), snapshot_base_path);
  }

  bool priv_snapshot_diff_data(const char *snapshot_base_path) const {
    // This is the first time to snapshot, just copy all
    if (!util::file_exist(priv_make_file_name(snapshot_base_path, k_segment_file_name))) {
      return priv_snapshot_entire_data(snapshot_base_path) && reset_soft_dirty_bit();
    }

    const size_type snapshot_no = priv_find_next_snapshot_no(snapshot_base_path);
    assert(snapshot_no > 0);

    const std::string base_file_name = priv_make_snapshot_base_file_name(snapshot_base_path, snapshot_no);

    // We take diff only for the segment data
    if (!priv_copy_backing_file(m_file_base_path.c_str(), base_file_name.c_str(), k_chunk_directory_file_name) ||
        !priv_copy_backing_file(m_file_base_path.c_str(), base_file_name.c_str(), k_bin_directory_file_name) ||
        !priv_copy_backing_file(m_file_base_path.c_str(), base_file_name.c_str(), k_named_object_directory_file_name)) {
      std::cerr << "Cannot snapshot backing file" << std::endl;
      return false;
    }

    return priv_snapshot_segment_diff(base_file_name.c_str()) && reset_soft_dirty_bit();
  }

  static std::string priv_make_snapshot_base_file_name(const std::string &base_name,
                                                       const size_type snapshot_no) {
    return base_name + "_" + k_diff_snapshot_file_name_prefix + "_" + std::to_string(snapshot_no);
  }

  static std::vector<std::string> priv_make_all_snapshot_base_file_names(const std::string &base_name,
                                                                         const size_type max_snapshot_no) {

    std::vector<std::string> list;
    list.reserve(max_snapshot_no - k_min_snapshot_no + 1);
    for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
      const std::string base_file_name = priv_make_snapshot_base_file_name(base_name, snapshop_no);
      list.emplace_back(base_file_name);
    }

    return list;
  }

  static bool reset_soft_dirty_bit() {
    if (!util::reset_soft_dirty_bit()) {
      std::cerr << "Failed to reset soft-dirty bit" << std::endl;
      return false;
    }
    return true;
  }

  /// \brief Assume the minimum snapshot number is 1
  /// \param snapshot_base_path
  /// \return Returns 0 on error
  static size_type priv_find_next_snapshot_no(const char *snapshot_base_path) {

    for (size_type snapshot_no = k_min_snapshot_no; snapshot_no < k_snapshot_no_safeguard; ++snapshot_no) {
      const auto file_name = priv_make_file_name(priv_make_snapshot_base_file_name(snapshot_base_path, snapshot_no),
                                                 k_segment_file_name);
      if (!util::file_exist(file_name)) {
        return snapshot_no;
      }
    }

    std::cerr << "There are already too many snapshots: " << k_snapshot_no_safeguard << std::endl;
    return 0;
  }

  bool priv_snapshot_segment_diff(const char *snapshot_base_file_name) const {
    const auto soft_dirty_page_no_list = priv_get_soft_dirty_page_no_list();

    const auto segment_diff_file_name = priv_make_file_name(snapshot_base_file_name, k_segment_file_name);
    std::ofstream segment_diff_file(segment_diff_file_name, std::ios::binary);
    if (!segment_diff_file) {
      std::cerr << "Cannot create a file: " << segment_diff_file_name << std::endl;
      return false;
    }

    {
      size_type buf = soft_dirty_page_no_list.size();
      segment_diff_file.write(reinterpret_cast<char *>(&buf), sizeof(size_type));
    }

    for (size_type page_no : soft_dirty_page_no_list) {
      segment_diff_file.write(reinterpret_cast<char *>(&page_no), sizeof(size_type));
    }

    const ssize_t page_size = util::get_page_size();
    assert(page_size > 0);

    for (const auto page_no : soft_dirty_page_no_list) {
      const char *const segment = static_cast<char *>(m_segment_storage.segment());
      segment_diff_file.write(&segment[page_no * page_size], page_size);
    }

    if (!segment_diff_file) {
      std::cerr << "Cannot write data to " << segment_diff_file_name << std::endl;
      return false;
    }

    segment_diff_file.close();

    return true;
  }

  auto priv_get_soft_dirty_page_no_list() const {
    std::vector<ssize_t> list;

    const ssize_t page_size = util::get_page_size();
    assert(page_size > 0);

    const size_type num_used_pages = m_chunk_directory.size() * k_chunk_size / page_size;
    const size_type page_no_offset = reinterpret_cast<uint64_t>(m_segment_storage.segment()) / page_size;
    for (size_type page_no = 0; page_no < num_used_pages; ++page_no) {
      util::pagemap_reader pr;
      const auto pagemap_value = pr.at(page_no_offset + page_no);
      if (pagemap_value == util::pagemap_reader::error_value) {
        std::cerr << "Cannot read pagemap at " << page_no
                  << " (" << (page_no_offset + page_no) * page_size << ")" << std::endl;
        return list;
      }
      if (util::check_soft_dirty_page(pagemap_value)) {
        list.emplace_back(page_no);
      }
    }

    return list;
  }

  auto priv_merge_segment_diff_list(const char *snapshot_base_path,
                                    const size_type max_snapshot_no) const {
    std::map<size_type, std::pair<size_type, size_type>> segment_diff_list;

    for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
      const std::string base_file_name = priv_make_snapshot_base_file_name(snapshot_base_path, snapshop_no);
      const auto segment_diff_file_name = priv_make_file_name(base_file_name, k_segment_file_name);

      std::ifstream ifs(segment_diff_file_name, std::ios::binary);
      if (!ifs.is_open()) {
        std::cerr << "Cannot open: " << segment_diff_file_name << std::endl;
        return segment_diff_list;
      }

      size_type num_diff_pages;
      ifs.read(reinterpret_cast<char *>(&num_diff_pages), sizeof(size_type));
      for (size_type offset = 0; offset < num_diff_pages; ++offset) {
        size_type page_no;
        ifs.read(reinterpret_cast<char *>(&page_no), sizeof(size_type));
        segment_diff_list[page_no] = std::make_pair(snapshop_no, offset);
      }
      if (!ifs.good()) {
        std::cerr << "Cannot read: " << segment_diff_file_name << std::endl;
      }
    }

    return segment_diff_list;
  }

  bool priv_apply_segment_diff(const char *snapshot_base_path,
                               const size_type max_snapshot_no,
                               const std::map<size_type, std::pair<size_type, size_type>> &segment_diff_list) {

    std::vector<std::ifstream *> diff_file_list(max_snapshot_no + 1, nullptr);
    std::vector<size_type> num_diff_list(max_snapshot_no + 1, 0);
    for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
      const std::string base_file_name = priv_make_snapshot_base_file_name(snapshot_base_path, snapshop_no);
      const auto segment_diff_file_name = priv_make_file_name(base_file_name, k_segment_file_name);

      auto ifs = new std::ifstream(segment_diff_file_name, std::ios::binary);
      if (!ifs->is_open()) {
        std::cerr << "Cannot open: " << segment_diff_file_name << std::endl;
        return false;
      }
      diff_file_list[snapshop_no] = ifs;

      size_type num_diff;
      ifs->read(reinterpret_cast<char *>(&num_diff), sizeof(size_type));
      num_diff_list[snapshop_no] = num_diff;
    }

    const ssize_t page_size = util::get_page_size();
    assert(page_size > 0);

    char *const segment = static_cast<char *>(m_segment_storage.segment());
    for (const auto &item : segment_diff_list) {
      const ssize_t page_no = item.first;
      assert(page_no * page_size < m_segment_storage.size());

      const ssize_t snapshot_no = item.second.first;
      assert(snapshot_no < diff_file_list.size());

      const ssize_t diff_no = item.second.second;
      assert(diff_no < num_diff_list[snapshot_no]);

      const ssize_t offset = diff_no + num_diff_list[snapshot_no] + 1;

      assert(diff_file_list[snapshot_no]->good());
      if (!diff_file_list[snapshot_no]->seekg(offset * sizeof(size_type))) {
        std::cerr << "Cannot seek to " << offset * sizeof(size_type) << " in " << snapshot_no << std::endl;
        return false;
      }

      assert(diff_file_list[snapshot_no]->good());
      if (!diff_file_list[snapshot_no]->read(&segment[page_no * page_size], page_size)) {
        std::cerr << "Cannot read diff page value at " << offset * sizeof(size_type) << " in " << snapshot_no
                  << std::endl;
        return false;
      }
    }

    for (const auto ifs : diff_file_list) {
      delete ifs;
    }

    return true;
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  std::string m_file_base_path;
  bin_directory_type m_bin_directory;
  chunk_directory_type m_chunk_directory;
  named_object_directory_type m_named_object_directory;
  segment_storage_type m_segment_storage;

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  mutex_type m_chunk_mutex;
  mutex_type m_named_object_directory_mutex;
  std::array<mutex_type, k_num_small_bins> m_bin_mutex;
#endif
};

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP

#include <metall/v0/kernel/manager_kernel_profile_impl.ipp>