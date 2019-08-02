// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_IMPL_IPP
#define METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_IMPL_IPP

#include <metall/v0/kernel/manager_kernel_fwd.hpp>

namespace metall {
namespace v0 {
namespace kernel {

// -------------------------------------------------------------------------------- //
// Constructor
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
manager_kernel<chnk_no, chnk_sz, alloc_t>::
manager_kernel(const manager_kernel<chnk_no, chnk_sz, alloc_t>::allocator_type &allocator)
    : m_file_base_path(),
      m_bin_directory(allocator),
      m_chunk_directory(allocator),
      m_named_object_directory(allocator),
      m_segment_storage() {}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
manager_kernel<chnk_no, chnk_sz, alloc_t>::~manager_kernel() {
  close();
}

// -------------------------------------------------------------------------------- //
// Public methods
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::create(const char *path, const size_type nbytes) {
  if (nbytes > k_max_size) {
    std::cerr << "Too large size was requested " << nbytes << " byte." << std::endl;
    std::cerr << "You should be able to fix this error by increasing k_max_size in " << __FILE__ << std::endl;
    std::abort();
  }

  m_file_base_path = path;

  if (!m_segment_storage.create(priv_make_file_name(k_segment_file_name).c_str(), nbytes)) {
    std::cerr << "Can not create segment" << std::endl;
    std::abort();
  }

  priv_init_segment_header();
  m_chunk_directory.allocate(priv_num_chunks());
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::open(const char *path) {
  m_file_base_path = path;

  if (!m_segment_storage.open(priv_make_file_name(k_segment_file_name).c_str())) {
    return false; // Note: this is not an fatal error due to open_or_create mode
  }
  priv_init_segment_header();

  m_chunk_directory.allocate(priv_num_chunks());

  const auto snapshot_no = priv_find_next_snapshot_no(path);
  if (snapshot_no == 0) {
    std::abort();
  } else if (snapshot_no == k_min_snapshot_no) { // Open normal files, i.e., not diff snapshot
    return priv_deserialize_management_data(path);
  } else { // Open diff snapshot files
    const auto diff_pages_list = priv_merge_segment_diff_list(path, snapshot_no - 1);
    if (!priv_apply_segment_diff(path, snapshot_no - 1, diff_pages_list)) {
      std::cerr << "Cannot apply segment diff" << std::endl;
      std::abort();
    }
    return priv_deserialize_management_data(priv_make_snapshot_base_file_name(path, snapshot_no - 1).c_str());
  }

  assert(false);
  return false;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::close() {
  if (priv_initialized()) {
    sync(true);
    m_segment_storage.destroy();
  }
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::sync(const bool sync) {
  assert(priv_initialized());

  m_segment_storage.sync(sync);
  priv_serialize_management_data();
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
allocate(const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type nbytes) {
  assert(priv_initialized());

  const bin_no_type bin_no = bin_no_mngr::to_bin_no(nbytes);

  if (bin_no < k_num_small_bins) {
    return priv_allocate_small_object(bin_no);
  }

  return priv_allocate_large_object(bin_no);
}

// \TODO: implement
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
allocate_aligned(const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type,
                 const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type) {
  assert(false);
  return nullptr;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::deallocate(void *addr) {
  assert(priv_initialized());

  if (!addr) return;

  const difference_type offset = static_cast<char *>(addr) - static_cast<char *>(m_segment_storage.get_segment());
  const chunk_no_type chunk_no = offset / k_chunk_size;
  const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);

  if (bin_no < k_num_small_bins) {
    priv_deallocate_small_object(offset, chunk_no, bin_no);
  } else {
    priv_deallocate_large_object(chunk_no, bin_no);
  }
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
template <typename T>
std::pair<T *, typename manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type>
manager_kernel<chnk_no, chnk_sz, alloc_t>::find(char_ptr_holder_type name) {
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
  return std::make_pair(reinterpret_cast<T *>(offset + static_cast<char *>(m_segment_storage.get_segment())), length);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::destroy(char_ptr_holder_type name) {
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
  auto ptr = static_cast<T *>(static_cast<void *>(offset + static_cast<char *>(m_segment_storage.get_segment())));
  for (size_type i = 0; i < length; ++i) {
    ptr->~T();
    ++ptr;
  }

  deallocate(offset + static_cast<char *>(m_segment_storage.get_segment()));

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
template <typename T>
T *manager_kernel<chnk_no, chnk_sz, alloc_t>::generic_construct(char_ptr_holder_type name,
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

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
typename manager_kernel<chnk_no, chnk_sz, alloc_t>::segment_header_type *
manager_kernel<chnk_no, chnk_sz, alloc_t>::get_segment_header() const {
  return reinterpret_cast<segment_header_type *>(m_segment_storage.get_header());
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::snapshot(const char *destination_base_path) {
  sync(true);
  return priv_snapshot_entire_data(destination_base_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::snapshot_diff(const char *destination_base_path) {
  sync(true);
  return priv_snapshot_diff_data(destination_base_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::copy_file(const char *source_base_path,
                                                          const char *destination_base_path) {
  return priv_copy_all_backing_files(source_base_path, destination_base_path,
                                     priv_find_next_snapshot_no(source_base_path) - 1);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::future<bool>
manager_kernel<chnk_no, chnk_sz, alloc_t>::copy_file_async(const char *source_base_path,
                                                           const char *destination_base_path) {
  return std::async(std::launch::async, copy_file, source_base_path, destination_base_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::remove_file(const char *base_path) {
  return priv_remove_all_backing_files(base_path, priv_find_next_snapshot_no(base_path) - 1);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::future<bool> manager_kernel<chnk_no, chnk_sz, alloc_t>::remove_file_async(const char *base_path) {
  return std::async(std::launch::async, remove_file, base_path);
}

// -------------------------------------------------------------------------------- //
// Private methods (not designed to be used by the base class)
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::string
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_make_file_name(const std::string &base_name,
                                                               const std::string &item_name) {
  return base_name + "_" + item_name;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_initialized() const {
  return (m_segment_storage.get_header() && m_segment_storage.get_segment() && m_segment_storage.size()
      && !m_file_base_path.empty());
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::string
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_make_file_name(const std::string &item_name) const {
  return priv_make_file_name(m_file_base_path, item_name);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
typename manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_num_chunks() const {
  return m_segment_storage.size() / k_chunk_size;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_init_segment_header() {
  new(get_segment_header()) segment_header_type();
  get_segment_header()->manager_kernel_address = this;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
template <typename T>
T *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_generic_named_construct(const char_type *const name,
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
        return reinterpret_cast<T *>(offset + static_cast<char *>(m_segment_storage.get_segment()));
      } else {
        return nullptr;
      }
    }

    ptr = allocate(num * sizeof(T));

    // Insert into named_object_directory
    const auto insert_ret = m_named_object_directory.insert(name,
                                                            static_cast<char *>(ptr)
                                                                - static_cast<char *>(m_segment_storage.get_segment()),
                                                            num);
    if (!insert_ret) {
      std::cerr << "Failed to insert a new name: " << name << std::endl;
      return nullptr;
    }
  }

  util::array_construct(ptr, num, table);

  return static_cast<T *>(ptr);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_free_chunk(const chunk_no_type head_chunk_no, const size_type num_chunks) {
  const off_t offset = head_chunk_no * k_chunk_size;
  const size_type length = num_chunks * k_chunk_size;
  m_segment_storage.free_region(offset, length);
}

// ---------------------------------------- For allocation ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_allocate_small_object(const bin_no_type bin_no) {
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

  assert(!m_chunk_directory.all_slots_marked(chunk_no));
  const chunk_slot_no_type chunk_slot_no = m_chunk_directory.find_and_mark_slot(chunk_no);

  if (m_chunk_directory.all_slots_marked(chunk_no)) {
    m_bin_directory.pop(bin_no);
  }

  const difference_type offset = k_chunk_size * chunk_no + object_size * chunk_slot_no;
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_allocate_large_object(const bin_no_type bin_no) {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  lock_guard_type chunk_guard(m_chunk_mutex);
#endif
  const chunk_no_type new_chunk_no = m_chunk_directory.insert(bin_no);
  const difference_type offset = k_chunk_size * new_chunk_no;
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

// ---------------------------------------- For deallocation ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_deallocate_small_object(const difference_type offset,
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
    {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type chunk_guard(m_chunk_mutex);
#endif
      m_chunk_directory.erase(chunk_no);
      priv_free_chunk(chunk_no, 1);
    }
    m_bin_directory.erase(bin_no, chunk_no);
  }
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_deallocate_large_object(const chunk_no_type chunk_no, const bin_no_type bin_no) {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  lock_guard_type chunk_guard(m_chunk_mutex);
#endif
  m_chunk_directory.erase(chunk_no);
  const size_type num_chunks = bin_no_mngr::to_object_size(bin_no) / k_chunk_size;
  priv_free_chunk(chunk_no, num_chunks);
}

// ---------------------------------------- For serializing/deserializing ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_serialize_management_data() {
  if (!m_bin_directory.serialize(priv_make_file_name(k_bin_directory_file_name).c_str()) ||
      !m_chunk_directory.serialize(priv_make_file_name(k_chunk_directory_file_name).c_str()) ||
      !m_named_object_directory.serialize(priv_make_file_name(k_named_object_directory_file_name).c_str())) {
    std::cerr << "Failed to serialize internal data" << std::endl;
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_deserialize_management_data(const char *const base_path) {
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
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_copy_backing_file(const char *const src_base_name,
                                                                  const char *const dst_base_name,
                                                                  const char *const item_name) {
  if (!util::copy_file(priv_make_file_name(src_base_name, item_name),
                       priv_make_file_name(dst_base_name, item_name))) {
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_copy_all_backing_files(const char *const src_base_name,
                                                                       const char *const dst_base_name) {
  bool ret = true;
  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_segment_file_name);
  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_bin_directory_file_name);
  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_chunk_directory_file_name);
  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_named_object_directory_file_name);

  return ret;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_copy_all_backing_files(const char *const src_base_name,
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

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_remove_backing_file(const char *const base_name, const char *const item_name) {
  if (!util::remove_file(priv_make_file_name(base_name, item_name))) {
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_remove_all_backing_files(const char *const base_name) {
  bool ret = true;

  ret &= priv_remove_backing_file(base_name, k_segment_file_name);
  ret &= priv_remove_backing_file(base_name, k_chunk_directory_file_name);
  ret &= priv_remove_backing_file(base_name, k_bin_directory_file_name);
  ret &= priv_remove_backing_file(base_name, k_named_object_directory_file_name);

  return ret;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_remove_all_backing_files(const char *const base_name,
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
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_snapshot_entire_data(const char *snapshot_base_path) const {
  return priv_copy_all_backing_files(m_file_base_path.c_str(), snapshot_base_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_snapshot_diff_data(const char *snapshot_base_path) const {
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

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::string
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_make_snapshot_base_file_name(const std::string &base_name,
                                  const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type snapshot_no) {
  return base_name + "_" + k_diff_snapshot_file_name_prefix + "_" + std::to_string(snapshot_no);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::vector<std::string>
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_make_all_snapshot_base_file_names(const std::string &base_name,
                                       const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type max_snapshot_no) {

  std::vector<std::string> list;
  list.reserve(max_snapshot_no - k_min_snapshot_no + 1);
  for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
    const std::string base_file_name = priv_make_snapshot_base_file_name(base_name, snapshop_no);
    list.emplace_back(base_file_name);
  }

  return list;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::reset_soft_dirty_bit() {
  if (!util::reset_soft_dirty_bit()) {
    std::cerr << "Failed to reset soft-dirty bit" << std::endl;
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
typename manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_find_next_snapshot_no(const char *snapshot_base_path) {

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

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_snapshot_segment_diff(const char *snapshot_base_file_name) const {
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
    const char *const segment = static_cast<char *>(m_segment_storage.get_segment());
    segment_diff_file.write(&segment[page_no * page_size], page_size);
  }

  if (!segment_diff_file) {
    std::cerr << "Cannot write data to " << segment_diff_file_name << std::endl;
    return false;
  }

  segment_diff_file.close();

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
auto manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_get_soft_dirty_page_no_list() const {
  std::vector<ssize_t> list;

  const ssize_t page_size = util::get_page_size();
  assert(page_size > 0);

  const size_type num_used_pages = m_chunk_directory.size() * k_chunk_size / page_size;
  const size_type page_no_offset = reinterpret_cast<uint64_t>(m_segment_storage.get_segment()) / page_size;
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

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
auto
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_merge_segment_diff_list(const char *snapshot_base_path,
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
    for (size_type diff_page_index = 0; diff_page_index < num_diff_pages; ++diff_page_index) {
      size_type page_no;
      ifs.read(reinterpret_cast<char *>(&page_no), sizeof(size_type));
      segment_diff_list[page_no] = std::make_pair(snapshop_no, diff_page_index);
    }
    if (!ifs.good()) {
      std::cerr << "Cannot read: " << segment_diff_file_name << std::endl;
    }
  }

  return segment_diff_list;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::
priv_apply_segment_diff(const char *snapshot_base_path,
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

    ifs->read(reinterpret_cast<char *>(&num_diff_list[snapshop_no]), sizeof(size_type));
  }

  const ssize_t page_size = util::get_page_size();
  assert(page_size > 0);

  char *const segment = static_cast<char *>(m_segment_storage.get_segment());
  for (const auto &item : segment_diff_list) {
    const size_type page_no = item.first;
    assert(page_no * static_cast<size_type>(page_size) < m_segment_storage.size());

    const size_type snapshot_no = item.second.first;
    assert(snapshot_no < diff_file_list.size());

    const size_type diff_page_index = item.second.second;
    assert(diff_page_index < num_diff_list[snapshot_no]);

    const size_type
        offset = (num_diff_list[snapshot_no] + 1) * sizeof(size_type) // Skip the list of diff page numbers
        + diff_page_index * static_cast<size_type>(page_size); // Offset to the target page data

    assert(diff_file_list[snapshot_no]->good());
    if (!diff_file_list[snapshot_no]->seekg(offset)) {
      std::cerr << "Cannot seek to " << offset << " of " << snapshot_no << std::endl;
      return false;
    }

    assert(diff_file_list[snapshot_no]->good());
    if (!diff_file_list[snapshot_no]->read(&segment[page_no * static_cast<size_type>(page_size)], page_size)) {
      std::cerr << "Cannot read diff page value at " << offset << " of " << snapshot_no << std::endl;
      return false;
    }
  }

  for (const auto ifs : diff_file_list) {
    delete ifs;
  }

  return true;
}
} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_IMPL_IPP

