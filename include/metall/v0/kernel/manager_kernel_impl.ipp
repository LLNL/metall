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
    : m_base_dir_path(),
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
void manager_kernel<chnk_no, chnk_sz, alloc_t>::create(const char *base_dir_path, const size_type vm_reserve_size) {
  if (vm_reserve_size > k_max_segment_size) {
    std::cerr << "Too large VM region size is requested " << vm_reserve_size << " byte." << std::endl;
    std::abort();
  }

  m_base_dir_path = base_dir_path;

  if (!priv_init_datastore_directory(base_dir_path)) {
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
  if (!m_segment_storage.create(priv_make_file_name(m_base_dir_path, k_segment_prefix),
                                m_vm_region_size - size_for_header,
                                static_cast<char *>(m_vm_region) + size_for_header,
                                k_initial_segment_size)) {
    std::cerr << "Cannot create application data segment" << std::endl;
    std::abort();
  }
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::open(const char *base_dir_path,
                                                     const bool read_only,
                                                     const size_type vm_reserve_size) {
  // Check if there is a file that can be used
  if (!m_segment_storage.openable(priv_make_file_name(base_dir_path, k_segment_prefix))) {
    return false; // This is not an fatal error due to the open_or_create mode
  }

  m_base_dir_path = base_dir_path;

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    std::abort();
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    std::abort();
  }

  const size_type offset = m_segment_header_size
      + (reinterpret_cast<char *>(m_segment_header) - reinterpret_cast<char *>(m_vm_region));
  if (!m_segment_storage.open(priv_make_file_name(m_base_dir_path, k_segment_prefix),
                              m_vm_region_size - offset,
                              static_cast<char *>(m_vm_region) + offset,
                              read_only)) {
    std::abort();
  }

  if (!priv_deserialize_management_data()) {
    std::abort();
  }

  return true;
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
  priv_serialize_management_data(); // Not to serialize
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
allocate(const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type nbytes) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return nullptr;

  const auto offset = m_segment_memory_allocator.allocate(nbytes);
  assert(offset >= 0);
  assert(offset + nbytes <= m_segment_storage.size());
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void *
manager_kernel<chnk_no, chnk_sz, alloc_t>::
allocate_aligned(const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type nbytes,
                 const manager_kernel<chnk_no, chnk_sz, alloc_t>::size_type alignment) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return nullptr;
  const auto offset = m_segment_memory_allocator.allocate_aligned(nbytes, alignment);
  assert(offset >= 0);
  assert(offset + nbytes <= m_segment_storage.size());
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
void manager_kernel<chnk_no, chnk_sz, alloc_t>::deallocate(void *addr) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return;
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

  if (m_segment_storage.read_only()) return false;

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
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::snapshot(const char *destination_base_dir_path) {
  assert(priv_initialized());
  sync(true);
  return priv_copy_data_store(m_base_dir_path, destination_base_dir_path, true);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::copy(const char *source_base_dir_path,
                                                     const char *destination_base_dir_path) {
  return priv_copy_data_store(source_base_dir_path, destination_base_dir_path, true);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::future<bool>
manager_kernel<chnk_no, chnk_sz, alloc_t>::copy_async(const char *source_dir_path,
                                                      const char *destination_dir_path) {
  return std::async(std::launch::async, copy, source_dir_path, destination_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::remove(const char *dir_path) {
  return priv_remove_data_store(dir_path);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::future<bool> manager_kernel<chnk_no, chnk_sz, alloc_t>::remove_async(const char *dir_path) {
  return std::async(std::launch::async, remove, dir_path);
}

// -------------------------------------------------------------------------------- //
// Private methods (not designed to be used by the base class)
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::string
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_make_datastore_dir_path(const std::string &base_dir_path) {
  return base_dir_path + "/" + k_datastore_dir_name;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
std::string
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_make_file_name(const std::string &base_dir_path,
                                                               const std::string &item_name) {
  return priv_make_datastore_dir_path(base_dir_path) + "/" + item_name;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_init_datastore_directory(const std::string &base_dir_path) {
  // Create the base directory if needed
  if (!util::file_exist(base_dir_path)) {
    if (!util::create_directory(base_dir_path)) {
      std::cerr << "Failed to create directory: " << base_dir_path << std::endl;
      return false;
    }
  }

  // Create the datastore directory if needed
  if (!util::file_exist(priv_make_datastore_dir_path(base_dir_path))) {
    if (!util::create_directory(priv_make_datastore_dir_path(base_dir_path))) {
      std::cerr << "Failed to create directory: " << priv_make_datastore_dir_path(base_dir_path) << std::endl;
      return false;
    }
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_initialized() const {
  assert(!m_base_dir_path.empty());
  assert(m_segment_storage.get_segment());
  return (m_vm_region && m_vm_region_size > 0 && m_segment_header && m_segment_storage.size() > 0);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_reserve_vm_region(const size_type nbytes) {
  // Align to the page size of the mmap implementation, which could be different from the system page size 
  const auto alignment = m_segment_storage.page_size();
  assert(alignment > 0);
  m_vm_region_size = util::round_up(nbytes, alignment);
  m_vm_region = util::reserve_aligned_vm_region(alignment, m_vm_region_size);
  if (!m_vm_region || m_vm_region % alignment != 0) {
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
  assert(priv_initialized());

  if (m_segment_storage.read_only()) return false;

  if (!m_named_object_directory.serialize(priv_make_file_name(m_base_dir_path,
                                                              k_named_object_directory_prefix).c_str())) {
    std::cerr << "Failed to serialize named object directory" << std::endl;
    return false;
  }
  if (!m_segment_memory_allocator.serialize(priv_make_file_name(m_base_dir_path, k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_deserialize_management_data() {
  if (!m_named_object_directory.deserialize(priv_make_file_name(m_base_dir_path,
                                                                k_named_object_directory_prefix).c_str())) {
    std::cerr << "Failed to deserialize named object directory" << std::endl;
    return false;
  }
  if (!m_segment_memory_allocator.deserialize(priv_make_file_name(m_base_dir_path,
                                                                  k_segment_memory_allocator_prefix))) {
    return false;
  }
  return true;
}

// ---------------------------------------- File operations ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_copy_data_store(const std::string &src_base_dir_path,
                                                                const std::string &dst_base_dir_path,
                                                                [[maybe_unused]] const bool overwrite) {
  const std::string src_datastore_dir_path = priv_make_datastore_dir_path(src_base_dir_path);
  if (!util::directory_exist(src_datastore_dir_path)) {
    std::cerr << "Source directory does not exist: " << src_datastore_dir_path << std::endl;
    return false;
  }

  if (!util::file_exist(dst_base_dir_path)) {
    if (!util::create_directory(dst_base_dir_path)) {
      std::cerr << "Failed to create directory: " << dst_base_dir_path << std::endl;
      return false;
    }
  }

  const std::string dst_datastore_dir_path = priv_make_datastore_dir_path(dst_base_dir_path);

  assert(*(src_datastore_dir_path.end()) != '/');
  return util::clone_file(src_datastore_dir_path, dst_datastore_dir_path, true);
}

template <typename chnk_no, std::size_t chnk_sz, typename alloc_t>
bool
manager_kernel<chnk_no, chnk_sz, alloc_t>::priv_remove_data_store(const std::string &dir_path) {
  if (!util::directory_exist(dir_path)) {
    return false;
  }
  return util::remove_file(dir_path);
}

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_IMPL_IPP
