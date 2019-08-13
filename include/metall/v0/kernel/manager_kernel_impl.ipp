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
manager_kernel(const manager_kernel<chnk_no, chnk_sz, alloc_t>::internal_data_allocator_type &allocator)
    : m_dir_path(),
      m_vm_region_size(0),
      m_vm_region(nullptr),
      m_segment_header_size(0),
      m_segment_header(nullptr),
      m_named_object_directory(allocator),
      m_segment_storage(),
      m_segment_memory_allocator(&m_segment_storage, allocator)
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    , m_named_object_directory_mutex()
#endif
{}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
manager_kernel<chnk_no, chnk_sz, alloc_t>::~manager_kernel() {
  close();
}

// -------------------------------------------------------------------------------- //
// Public methods
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::create(const char *dir_path, const size_type vm_reserve_size) {
  if (vm_reserve_size > k_max_segment_size) {
    std::cerr << "Too large VM region size is requested " << vm_reserve_size << " byte." << std::endl;
    std::abort();
  }

  if (!priv_set_up_datastore_directory(dir_path) ){
    std::abort();
  }

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    std::abort();
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    std::abort();
  }

  const size_type size_for_header = m_segment_header_size
      + (reinterpret_cast<char *>(m_segment_header) - reinterpret_cast<char *>(m_vm_region));
  if (!m_segment_storage.create(priv_make_file_name(m_dir_path, k_segment_prefix),
                                m_vm_region_size - size_for_header,
                                static_cast<char *>(m_vm_region) + size_for_header,
                                k_initial_segment_size)) {
    std::cerr << "Cannot create application data segment" << std::endl;
    std::abort();
  }
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::open(const char *dir_path, const size_type vm_reserve_size) {
  if (!priv_set_up_datastore_directory(dir_path) ){
    std::abort();
  }

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    std::abort();
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    std::abort();
  }

  const size_type offset = m_segment_header_size
      + (reinterpret_cast<char *>(m_segment_header) - reinterpret_cast<char *>(m_vm_region));
  if (!m_segment_storage.open(priv_make_file_name(m_dir_path, k_segment_prefix),
                              m_vm_region_size - offset,
                              static_cast<char *>(m_vm_region) + offset)) {
    return false; // Note: this is not an fatal error due to open_or_create mode
  }

  const auto snapshot_no = priv_find_next_snapshot_no(m_dir_path.c_str());
  if (snapshot_no == 0) {
    std::abort();
  } else if (snapshot_no == k_min_snapshot_no) { // Open normal files, i.e., not diff snapshot
    return priv_deserialize_management_data(m_dir_path.c_str());
  } else { // Open diff snapshot files
    const auto diff_pages_list = priv_merge_segment_diff_list(m_dir_path.c_str(), snapshot_no - 1);
    if (!priv_apply_segment_diff(m_dir_path.c_str(), snapshot_no - 1, diff_pages_list)) {
      std::cerr << "Cannot apply segment diff" << std::endl;
      std::abort();
    }
    return priv_deserialize_management_data(priv_make_snapshot_base_file_name(m_dir_path.c_str(), snapshot_no - 1).c_str());
  }

  assert(false);
  return false;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::close() {
  if (priv_initialized()) {
    priv_serialize_management_data();
    m_segment_storage.sync(true);
    m_segment_storage.destroy();
    priv_deallocate_segment_header();
    priv_release_vm_region();
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
  const auto offset = m_segment_memory_allocator.allocate(nbytes);
  assert(offset + nbytes <= m_segment_storage.size());
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
allocate_aligned(const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type nbytes,
                 const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type alignment) {
  const auto offset = m_segment_memory_allocator.allocate_aligned(nbytes);
  assert(offset + nbytes <= m_segment_storage.size());
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::deallocate(void *addr) {
  assert(priv_initialized());
  if (!addr) return;
  const difference_type offset = static_cast<char *>(addr) - static_cast<char *>(m_segment_storage.get_segment());
  m_segment_memory_allocator.deallocate(offset);
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
  return reinterpret_cast<segment_header_type *>(m_segment_header);
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
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_set_up_datastore_directory(const std::string &dir_path) {
  if (!util::file_exist(dir_path)) {
    if(!util::create_dir(dir_path)) {
      std::cerr << "Failed to create directory: " << dir_path << std::endl;
      return false;
    }
  }

  const std::string data_store_dir_name(dir_path + "/" + k_datastore_dir_name);
  if (!util::file_exist(data_store_dir_name)) {
    if (!util::create_dir(data_store_dir_name)) {
      std::cerr << "Failed to create directory: " << data_store_dir_name << std::endl;
      return false;
    }
  }

  m_dir_path = data_store_dir_name + "/";

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_initialized() const {
  assert(!m_dir_path.empty());
  assert(m_segment_storage.get_segment());
  return (m_vm_region && m_vm_region_size > 0 && m_segment_header && m_segment_storage.size() > 0);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_reserve_vm_region(const size_type nbytes) {
  const auto page_size = util::get_page_size();
  assert(page_size > 0);
  m_vm_region_size = util::round_up(nbytes, page_size);
  m_vm_region = util::reserve_vm_region(m_vm_region_size);
  if (!m_vm_region) {
    std::cerr << "Cannot reserve a VM region " << nbytes << " bytes" << std::endl;
    m_vm_region_size = 0;
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_release_vm_region() {
  const auto ret = util::munmap(m_vm_region, m_vm_region_size, false);
  m_vm_region = nullptr;
  m_vm_region_size = 0;
  return ret;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_allocate_segment_header(void *const addr) {

  if (!addr) {
    return false;
  }
  const auto page_size = util::get_page_size();
  if (page_size <= 0) {
    std::cerr << "Failed to get system page size" << std::endl;
    return false;
  }

  m_segment_header_size = util::round_up(sizeof(segment_header_type), page_size);
  if (util::map_anonymous_write_mode(addr, m_segment_header_size, MAP_FIXED) != addr) {
    std::cerr << "Cannot allocate segment header" << std::endl;
    return false;
  }
  m_segment_header = reinterpret_cast<segment_header_type *>(addr);

  new(m_segment_header) segment_header_type();
  m_segment_header->manager_kernel_address = this;

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_deallocate_segment_header() {
  m_segment_header->~segment_header_type();
  const auto ret = util::munmap(m_segment_header, m_segment_header_size, false);
  m_segment_header = nullptr;
  m_segment_header_size = 0;
  return ret;
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

// ---------------------------------------- For serializing/deserializing ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_serialize_management_data() {
  if (!m_named_object_directory.serialize(priv_make_file_name(m_dir_path,
                                                              k_named_object_directory_prefix).c_str())) {
    std::cerr << "Failed to serialize named object directory" << std::endl;
    return false;
  }
  if (!m_segment_memory_allocator.serialize(priv_make_file_name(m_dir_path, k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_deserialize_management_data(const char *const base_path) {
  if (!m_named_object_directory.deserialize(priv_make_file_name(base_path, k_named_object_directory_prefix).c_str())) {
    std::cerr << "Failed to deserialize named object directory" << std::endl;
    return false;
  }
  if (!m_segment_memory_allocator.deserialize(priv_make_file_name(base_path, k_segment_memory_allocator_prefix))) {
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
  assert(false); // TODO IMPLEMENT!!!
//  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_segment_prefix);
//  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_bin_directory_file_name);
//  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_chunk_directory_file_name);
//  ret &= priv_copy_backing_file(src_base_name, dst_base_name, k_named_object_directory_prefix);

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

  ret &= priv_remove_backing_file(base_name, k_segment_prefix);
  assert(false); // TODO IMPLEMENT!!!
//  ret &= priv_remove_backing_file(base_name, k_chunk_directory_file_name);
//  ret &= priv_remove_backing_file(base_name, k_bin_directory_file_name);
  ret &= priv_remove_backing_file(base_name, k_named_object_directory_prefix);

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
  return priv_copy_all_backing_files(m_dir_path.c_str(), snapshot_base_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_snapshot_diff_data(const char *snapshot_base_path) const {
  // This is the first time to snapshot, just copy all
  if (!util::file_exist(priv_make_file_name(snapshot_base_path, k_segment_prefix))) {
    return priv_snapshot_entire_data(snapshot_base_path) && reset_soft_dirty_bit();
  }

  const size_type snapshot_no = priv_find_next_snapshot_no(snapshot_base_path);
  assert(snapshot_no > 0);

  const std::string base_file_name = priv_make_snapshot_base_file_name(snapshot_base_path, snapshot_no);

  assert(false); // TODO IMPLEMENT!!!
  // We take diff only for the segment data
//  if (!priv_copy_backing_file(m_dir_path.c_str(), base_file_name.c_str(), k_chunk_directory_file_name) ||
//      !priv_copy_backing_file(m_dir_path.c_str(), base_file_name.c_str(), k_bin_directory_file_name) ||
//      !priv_copy_backing_file(m_dir_path.c_str(), base_file_name.c_str(), k_named_object_directory_prefix)) {
//    std::cerr << "Cannot snapshot backing file" << std::endl;
//    return false;
//  }

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
                                               k_segment_prefix);
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

  const auto segment_diff_file_name = priv_make_file_name(snapshot_base_file_name, k_segment_prefix);
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

  assert(false); // TODO IMPLEMENT!!!
//  const size_type num_used_pages = m_chunk_directory.size() * k_chunk_size / page_size;
//  const size_type page_no_offset = reinterpret_cast<uint64_t>(m_segment_storage.get_segment()) / page_size;
//  for (size_type page_no = 0; page_no < num_used_pages; ++page_no) {
//    util::pagemap_reader pr;
//    const auto pagemap_value = pr.at(page_no_offset + page_no);
//    if (pagemap_value == util::pagemap_reader::error_value) {
//      std::cerr << "Cannot read pagemap at " << page_no
//                << " (" << (page_no_offset + page_no) * page_size << ")" << std::endl;
//      return list;
//    }
//    if (util::check_soft_dirty_page(pagemap_value)) {
//      list.emplace_back(page_no);
//    }
//  }

  return list;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
auto
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_merge_segment_diff_list(const char *snapshot_base_path,
                                                                        const size_type max_snapshot_no) const {
  std::map<size_type, std::pair<size_type, size_type>> segment_diff_list;

  for (size_type snapshop_no = k_min_snapshot_no; snapshop_no <= max_snapshot_no; ++snapshop_no) {
    const std::string base_file_name = priv_make_snapshot_base_file_name(snapshot_base_path, snapshop_no);
    const auto segment_diff_file_name = priv_make_file_name(base_file_name, k_segment_prefix);

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
    const auto segment_diff_file_name = priv_make_file_name(base_file_name, k_segment_prefix);

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

