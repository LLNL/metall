// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
#define METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP

#include <metall/kernel/manager_kernel_fwd.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {

// -------------------------------------------------------------------------------- //
// Constructor
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
manager_kernel<chnk_no, chnk_sz>::manager_kernel()
    : m_base_dir_path(),
      m_vm_region_size(0),
      m_vm_region(nullptr),
      m_segment_header(nullptr),
      m_named_object_directory(),
      m_unique_object_directory(),
      m_anonymous_object_directory(),
      m_segment_storage(),
      m_segment_memory_allocator(&m_segment_storage),
      m_manager_metadata()
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    , m_object_directories_mutex()
#endif
{
  priv_validate_runtime_configuration();
}

template <typename chnk_no, std::size_t chnk_sz>
manager_kernel<chnk_no, chnk_sz>::~manager_kernel() noexcept {
  close();
}

// -------------------------------------------------------------------------------- //
// Public methods
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::create(const char *base_dir_path, const size_type vm_reserve_size) {
  return priv_create(base_dir_path, vm_reserve_size);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::open_read_only(const char *base_dir_path) {
  return priv_open(base_dir_path, true, 0);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::open(const char *base_dir_path,
                                            const size_type vm_reserve_size_request) {
  return priv_open(base_dir_path, false, vm_reserve_size_request);
}

template <typename chnk_no, std::size_t chnk_sz>
void manager_kernel<chnk_no, chnk_sz>::close() {
  if (priv_initialized()) {
    if (!m_segment_storage.read_only()) {
      priv_serialize_management_data();
      m_segment_storage.sync(true);
    }

    m_segment_storage.destroy();
    priv_deallocate_segment_header();
    priv_release_vm_region();

    if (!m_segment_storage.read_only()) {
      // This function must be called at the end
      priv_mark_properly_closed(m_base_dir_path);
    }
  }
}

template <typename chnk_no, std::size_t chnk_sz>
void manager_kernel<chnk_no, chnk_sz>::flush(const bool synchronous) {
  assert(priv_initialized());
  m_segment_storage.sync(synchronous);
}

template <typename chnk_no, std::size_t chnk_sz>
void *
manager_kernel<chnk_no, chnk_sz>::
allocate(const manager_kernel<chnk_no, chnk_sz>::size_type nbytes) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return nullptr;

  const auto offset = m_segment_memory_allocator.allocate(nbytes);
  assert(offset >= 0);
  assert(offset + nbytes <= m_segment_storage.size());
  return priv_to_address(offset);
}

template <typename chnk_no, std::size_t chnk_sz>
void *
manager_kernel<chnk_no, chnk_sz>::
allocate_aligned(const manager_kernel<chnk_no, chnk_sz>::size_type nbytes,
                 const manager_kernel<chnk_no, chnk_sz>::size_type alignment) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return nullptr;

  // This requirement could be removed, but it would need some work to do
  if (alignment > k_chunk_size) return nullptr;

  const auto offset = m_segment_memory_allocator.allocate_aligned(nbytes, alignment);
  if (offset == m_segment_memory_allocator.k_null_offset) {
    return nullptr;
  }
  assert(offset >= 0);
  assert(offset + nbytes <= m_segment_storage.size());

  auto *addr = priv_to_address(offset);
  assert((uint64_t)addr % alignment == 0);
  return addr;
}

template <typename chnk_no, std::size_t chnk_sz>
void manager_kernel<chnk_no, chnk_sz>::deallocate(void *addr) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return;
  if (!addr) return;
  m_segment_memory_allocator.deallocate(priv_to_offset(addr));
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
std::pair<T *, typename manager_kernel<chnk_no, chnk_sz>::size_type>
manager_kernel<chnk_no, chnk_sz>::find(char_ptr_holder_type name) const {
  assert(priv_initialized());

  if (name.is_anonymous()) {
    return std::make_pair(nullptr, 0);
  }

  if (name.is_unique()) {
    auto itr = m_unique_object_directory.find(typeid(T).name());
    if (itr != m_unique_object_directory.end()) {
      auto *const addr = reinterpret_cast<T *>(priv_to_address(itr->offset));
      const auto length = itr->length;
      return std::make_pair(addr, length);
    }
  } else {
    auto itr = m_named_object_directory.find(name.get());
    if (itr != m_named_object_directory.end()) {
      auto *const addr = reinterpret_cast<T *>(priv_to_address(itr->offset));
      const auto length = itr->length;
      return std::make_pair(addr, length);
    }
  }

  return std::make_pair(nullptr, 0);
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::destroy(char_ptr_holder_type name) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return false;

  if (name.is_anonymous()) {
    return false;
  }

  return priv_destroy_and_update_object_directory_by_name<T>(name);
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::destroy_ptr(const T *ptr) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return false;

  return priv_destroy_and_update_object_directory_by_offset<T>(priv_to_offset(ptr));
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
const typename manager_kernel<chnk_no, chnk_sz>::char_type *
manager_kernel<chnk_no, chnk_sz>::get_instance_name(const T *ptr) const {

  auto nitr = m_named_object_directory.find(priv_to_offset(ptr));
  if (nitr != m_named_object_directory.end()) {
    return nitr->name.c_str();
  }

  auto uitr = m_unique_object_directory.find(priv_to_offset(ptr));
  if (uitr != m_unique_object_directory.end()) {
    return uitr->name.c_str();
  }

  return 0; // This is not error, anonymous object or non-constructed object
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
typename manager_kernel<chnk_no, chnk_sz>::instance_type
manager_kernel<chnk_no, chnk_sz>::get_instance_type(const T *ptr) const {
  if (m_named_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_type::named_type;
  }

  if (m_unique_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_type::unique_type;
  }

  if (m_anonymous_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_type::anonymous_type;
  }

  logger::out(logger::level::critical, __FILE__, __LINE__, "Invalid pointer");
  return instance_type();
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_instance_length(const T *ptr) const {
  {
    auto itr = m_named_object_directory.find(priv_to_offset(ptr));
    if (itr != m_named_object_directory.end()) {
      assert(itr->length > 0);
      return itr->length;
    }
  }

  {
    auto itr = m_unique_object_directory.find(priv_to_offset(ptr));
    if (itr != m_unique_object_directory.end()) {
      assert(itr->length > 0);
      return itr->length;
    }
  }

  {
    auto itr = m_anonymous_object_directory.find(priv_to_offset(ptr));
    if (itr != m_anonymous_object_directory.end()) {
      assert(itr->length > 0);
      return itr->length;
    }
  }

  return 0; // Won't treat as an error
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_num_named_objects() const {
  return m_named_object_directory.size();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_num_unique_objects() const {
  return m_unique_object_directory.size();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::const_named_iterator
manager_kernel<chnk_no, chnk_sz>::named_begin() const {
  return m_named_object_directory.begin();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::const_named_iterator
manager_kernel<chnk_no, chnk_sz>::named_end() const {
  return m_named_object_directory.end();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::const_unique_iterator
manager_kernel<chnk_no, chnk_sz>::unique_begin() const {
  return m_unique_object_directory.begin();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::const_unique_iterator
manager_kernel<chnk_no, chnk_sz>::unique_end() const {
  return m_unique_object_directory.end();
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
T *manager_kernel<chnk_no, chnk_sz>::generic_construct(char_ptr_holder_type name,
                                                       const size_type num,
                                                       const bool try2find,
                                                       const bool dothrow,
                                                       util::in_place_interface &table) {
  assert(priv_initialized());
  return priv_construct_and_update_object_directory<T>(name, num, try2find, dothrow, table);
}

template <typename chnk_no, std::size_t chnk_sz>
const typename manager_kernel<chnk_no, chnk_sz>::segment_header_type *
manager_kernel<chnk_no, chnk_sz>::get_segment_header() const {
  return reinterpret_cast<segment_header_type *>(m_segment_header);
}

template <typename chnk_no, std::size_t chnk_sz>
const void *
manager_kernel<chnk_no, chnk_sz>::get_segment() const {
  return m_segment_storage.get_segment();
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::snapshot(const char *destination_base_dir_path) {
  assert(priv_initialized());
  m_segment_storage.sync(true);
  priv_serialize_management_data();

  if (!util::create_directory(priv_make_top_dir_path(destination_base_dir_path))) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to create directory: " + std::string(destination_base_dir_path));
    return false;
  }

  // Copy only core directory first
  if (!util::clone_file(priv_make_core_dir_path(m_base_dir_path),
                        priv_make_core_dir_path(destination_base_dir_path), true)) {
    std::stringstream ss;
    ss << "Failed to copy " << priv_make_top_dir_path(m_base_dir_path) << " to "
       << priv_make_top_dir_path(destination_base_dir_path);
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
  }

  // Make a new metadata
  json_store meta_data;
  if (!priv_set_uuid(&meta_data)) return false;
  if (!priv_set_version(&meta_data)) return false;
  if (!priv_write_management_metadata(destination_base_dir_path, meta_data)) return false;

  // Finally, mark it as properly-closed
  if (!priv_mark_properly_closed(destination_base_dir_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed to create a properly closed mark");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::copy(const char *source_base_dir_path,
                                            const char *destination_base_dir_path) {
  return priv_copy_data_store(source_base_dir_path, destination_base_dir_path, true);
}

template <typename chnk_no, std::size_t chnk_sz>
std::future<bool>
manager_kernel<chnk_no, chnk_sz>::copy_async(const char *source_dir_path,
                                             const char *destination_dir_path) {
  return std::async(std::launch::async, copy, source_dir_path, destination_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::remove(const char *base_dir_path) {
  return priv_remove_data_store(base_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
std::future<bool> manager_kernel<chnk_no, chnk_sz>::remove_async(const char *base_dir_path) {
  return std::async(std::launch::async, remove, base_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::consistent(const char *dir_path) {
  return priv_consistent(dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
std::string manager_kernel<chnk_no, chnk_sz>::get_uuid() const {
  return self_type::get_uuid(m_base_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
std::string manager_kernel<chnk_no, chnk_sz>::get_uuid(const char *dir_path) {
  json_store meta_data;
  if (!priv_read_management_metadata(dir_path, &meta_data)) {
    logger::out(logger::level::error,
                __FILE__,
                __LINE__,
                "Cannot read management metadata in " + std::string(dir_path));
    return "";
  }
  return priv_get_uuid(meta_data);
}

template <typename chnk_no, std::size_t chnk_sz>
version_type manager_kernel<chnk_no, chnk_sz>::get_version() const {
  return self_type::get_version(m_base_dir_path);
}

template <typename chnk_no, std::size_t chnk_sz>
version_type manager_kernel<chnk_no, chnk_sz>::get_version(const char *dir_path) {
  json_store meta_data;
  if (!priv_read_management_metadata(dir_path, &meta_data)) {
    logger::out(logger::level::error,
                __FILE__,
                __LINE__,
                "Cannot read management metadata in " + std::string(dir_path));
    return 0;
  }
  const auto version = priv_get_version(meta_data);
  return (version == detail::k_error_version) ? 0 : version;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::get_description(const std::string &base_dir_path, std::string *description) {
  return priv_read_description(base_dir_path, description);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::get_description(std::string *description) const {
  return priv_read_description(m_base_dir_path, description);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::set_description(const std::string &base_dir_path,
                                                         const std::string &description) {
  return priv_write_description(base_dir_path, description);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::set_description(const std::string &description) {
  return set_description(m_base_dir_path, description);
}

// -------------------------------------------------------------------------------- //
// Private methods
// -------------------------------------------------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::difference_type
manager_kernel<chnk_no, chnk_sz>::priv_to_offset(const void *const ptr) const {
  return static_cast<char *>(const_cast<void *>(ptr)) - static_cast<char *>(m_segment_storage.get_segment());
}

template <typename chnk_no, std::size_t chnk_sz>
void *manager_kernel<chnk_no, chnk_sz>::priv_to_address(const difference_type offset) const {
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_top_dir_path(const std::string &base_dir_path) {
  return base_dir_path + "/" + k_datastore_top_dir_name;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_top_level_file_name(const std::string &base_dir_path,
                                                                const std::string &item_name) {
  return priv_make_top_dir_path(base_dir_path) + "/" + item_name;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_core_dir_path(const std::string &base_dir_path) {
  return priv_make_top_dir_path(base_dir_path) + "/" + k_datastore_core_dir_name;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_core_file_name(const std::string &base_dir_path,
                                                           const std::string &item_name) {
  return priv_make_core_dir_path(base_dir_path) + "/" + item_name;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_init_datastore_directory(const std::string &base_dir_path) {
  // Create the base directory if needed
  if (!util::create_directory(base_dir_path)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to create directory: " + base_dir_path);
    return false;
  }

  // Remove existing directory to certainly create a new data store
  if (!remove(base_dir_path.c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to remove a directory: " + base_dir_path);
    return false;
  }

  // Create the data store directory if needed
  if (!util::create_directory(priv_make_core_dir_path(base_dir_path))) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Failed to create directory: " + priv_make_core_dir_path(base_dir_path));
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_initialized() const {
  assert(!m_base_dir_path.empty());
  assert(m_segment_storage.get_segment());
  return (m_vm_region && m_vm_region_size > 0 && m_segment_header && m_segment_storage.size() > 0);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_validate_runtime_configuration() const {
  const auto system_page_size = util::get_page_size();
  if (system_page_size <= 0) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to get the system page size");
    return false;
  }

  if (k_chunk_size % system_page_size != 0) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "The chunk size must be a multiple of the system page size");
    return false;
  }

  if (m_segment_storage.page_size() > k_chunk_size) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "The page size of the segment storage must be equal or smaller than the chunk size");
    return false;
  }

  if (m_segment_storage.page_size() % system_page_size != 0) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "The page size of the segment storage must be a multiple of the system page size");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_consistent(const std::string &base_dir_path) {
  json_store metadata;
  return priv_properly_closed(base_dir_path) && (priv_read_management_metadata(base_dir_path, &metadata)
      && priv_check_version(metadata));
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_check_version(const json_store &metadata_json) {
  return priv_get_version(metadata_json) == version_type(METALL_VERSION);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_properly_closed(const std::string &base_dir_path) {
  return util::file_exist(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_mark_properly_closed(const std::string &base_dir_path) {
  return util::create_file(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_unmark_properly_closed(const std::string &base_dir_path) {
  return util::remove_file(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_reserve_vm_region(const size_type nbytes) {
  // Align the VM region to the page size to decrease the implementation cost of some features, such as
  // supporting Umap and aligned allocation
  const auto alignment = k_chunk_size;

  assert(alignment > 0);
  m_vm_region_size = util::round_up(nbytes, alignment);
  m_vm_region = util::reserve_aligned_vm_region(alignment, m_vm_region_size);
  if (!m_vm_region) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Cannot reserve a VM region " + std::to_string(nbytes) + " bytes");
    m_vm_region_size = 0;
    return false;
  }
  assert(reinterpret_cast<uint64_t>(m_vm_region) % alignment == 0);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_release_vm_region() {

  if (!util::munmap(m_vm_region, m_vm_region_size, false)) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Cannot release a VM region " + std::to_string((uint64_t)m_vm_region) + ", "
                    + std::to_string(m_vm_region_size) + " bytes.");
    return false;
  }
  m_vm_region = nullptr;
  m_vm_region_size = 0;

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_allocate_segment_header(void *const addr) {

  if (!addr) {
    return false;
  }

  if (util::map_anonymous_write_mode(addr, k_segment_header_size, MAP_FIXED) != addr) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Cannot allocate segment header");
    return false;
  }
  m_segment_header = reinterpret_cast<segment_header_type *>(addr);

  new(m_segment_header) segment_header_type();
  m_segment_header->manager_kernel_address = this;

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_deallocate_segment_header() {
  m_segment_header->~segment_header_type();
  const auto ret = util::munmap(m_segment_header, k_segment_header_size, false);
  m_segment_header = nullptr;
  if (!ret) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed to deallocate segment header");
  }
  return ret;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
T *
manager_kernel<chnk_no, chnk_sz>::
priv_construct_and_update_object_directory(char_ptr_holder_type name,
                                           size_type length,
                                           bool try2find,
                                           bool, // TODO implement 'dothrow'
                                           util::in_place_interface &table) {
  void *ptr = nullptr;
  {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(m_object_directories_mutex);
#endif

    if (!name.is_anonymous()) {
      auto *const found_addr = find<T>(name).first;
      if (found_addr) {
        if (try2find) {
          return found_addr;
        }
        return nullptr; // this is not error always --- could have been allocated by another thread.
      }
    }

    ptr = allocate(length * sizeof(T));
    const auto offset = priv_to_offset(ptr);
    if (!priv_update_object_directory_no_mutex<T>(name, offset, length)) {
      deallocate(ptr);
      return nullptr; // Critical error
    }
  }

  // Construct each object in the allocated memory
  util::array_construct(ptr, length, table);

  return static_cast<T *>(ptr);
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::priv_update_object_directory_no_mutex(char_ptr_holder_type name,
                                                                             const difference_type offset,
                                                                             size_type length) {
  if (name.is_anonymous()) {
    if (!m_anonymous_object_directory.insert(std::to_string(offset), offset, length)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to insert an entry into the anonymous object table");
      return false;
    }
  } else if (name.is_unique()) {
    if (!m_unique_object_directory.insert(typeid(T).name(), offset, length)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to insert an entry into the unique object table");
      return false;
    }
  } else {
    if (!m_named_object_directory.insert(name.get(), offset, length)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to insert an entry into the named object table");
      return false;
    }
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::priv_destroy_and_update_object_directory_by_name(char_ptr_holder_type name) {
  T *addr = nullptr;
  size_type length = 0;

  {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(m_object_directories_mutex);
#endif

    std::tie(addr, length) = find<T>(name);
    if (!addr) {
      return false; // could have been destroyed by another thread.
    }

    if (!m_named_object_directory.erase(priv_to_offset(addr))
        && !m_unique_object_directory.erase(priv_to_offset(addr))
        && !m_anonymous_object_directory.erase(priv_to_offset(addr))) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to erase an entry from object directories");
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(priv_to_offset(addr), length);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::priv_destroy_and_update_object_directory_by_offset(difference_type offset) {
  size_type length = 0;

  {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(m_object_directories_mutex);
#endif
    length = get_instance_length(priv_to_address(offset));
    if (length == 0) {
      return false;
    }

    if (!m_named_object_directory.erase(offset)
        && !m_unique_object_directory.erase(offset)
        && !m_anonymous_object_directory.erase(offset)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to erase an entry from object directories");
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(offset, length);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
void manager_kernel<chnk_no, chnk_sz>::priv_destruct_and_free_memory(const difference_type offset,
                                                                     const size_type length) {
  auto *object = static_cast<T *>(priv_to_address(offset));
  // Destruct each object
  for (size_type i = 0; i < length; ++i) {
    object->~T();
    ++object;
  }
  // Finally, deallocate the memory
  m_segment_memory_allocator.deallocate(offset);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_open(const char *base_dir_path,
                                                 const bool read_only,
                                                 const size_type vm_reserve_size_request) {
  if (!priv_validate_runtime_configuration()) {
    return false;
  }

  if (!priv_read_management_metadata(base_dir_path, &m_manager_metadata)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to read management metadata");
    return false;
  }

  if (!priv_check_version(m_manager_metadata)) {
    std::stringstream ss;
    ss << "Invalid version — it was created by Metall v" << to_version_string(priv_get_version(m_manager_metadata))
       << " (currently using v" << to_version_string(METALL_VERSION) << ")";
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str());
    return false;
  }

  if (!priv_properly_closed(base_dir_path)) {
    logger::out(logger::level::critical, __FILE__, __LINE__,
                "Inconsistent data store — it was not closed properly and might have been collapsed.");
    return false;
  }

  m_base_dir_path = base_dir_path;

  const size_type existing_segment_size = segment_storage_type::get_size(priv_make_core_file_name(
      m_base_dir_path,
      k_segment_prefix));
  const size_type vm_reserve_size = (read_only) ? existing_segment_size + k_segment_header_size
                                                : std::max(existing_segment_size + k_segment_header_size,
                                                           vm_reserve_size_request);

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    return false;
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    priv_release_vm_region();
    return false;
  }

  // Clear the consistent mark before opening with the write mode
  if (!read_only && !priv_unmark_properly_closed(m_base_dir_path)) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Failed to erase the properly close mark before opening");
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  if (!m_segment_storage.open(priv_make_core_file_name(m_base_dir_path, k_segment_prefix),
                              m_vm_region_size - k_segment_header_size,
                              static_cast<char *>(m_vm_region) + k_segment_header_size,
                              read_only)) {
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  if (!priv_deserialize_management_data()) {
    m_segment_storage.destroy();
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_create(const char *base_dir_path,
                                                   const size_type vm_reserve_size) {
  if (!priv_validate_runtime_configuration()) {
    return false;
  }

  if (vm_reserve_size > k_max_segment_size) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Too large VM region size is requested " + std::to_string(vm_reserve_size) + " byte.");
    return false;
  }

  m_base_dir_path = base_dir_path;

  if (!priv_unmark_properly_closed(m_base_dir_path) || !priv_init_datastore_directory(base_dir_path)) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Failed to initialize datastore directory under " + std::string(base_dir_path));
    return false;
  }

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    return false;
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    priv_release_vm_region();
    return false;
  }

  if (!m_segment_storage.create(priv_make_core_file_name(m_base_dir_path, k_segment_prefix),
                                m_vm_region_size - k_segment_header_size,
                                static_cast<char *>(m_vm_region) + k_segment_header_size,
                                k_initial_segment_size)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Cannot create application data segment");
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  if (!priv_set_uuid(&m_manager_metadata) || !priv_set_version(&m_manager_metadata)
      || !priv_write_management_metadata(m_base_dir_path, m_manager_metadata)) {
    m_segment_storage.destroy();
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  return true;
}

// ---------------------------------------- For serializing/deserializing ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_serialize_management_data() {
  assert(priv_initialized());

  if (m_segment_storage.read_only()) return true;

  if (!m_named_object_directory.serialize(priv_make_core_file_name(m_base_dir_path,
                                                                   k_named_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.serialize(priv_make_core_file_name(m_base_dir_path,
                                                                    k_unique_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.serialize(priv_make_core_file_name(m_base_dir_path,
                                                                       k_anonymous_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.serialize(priv_make_core_file_name(m_base_dir_path,
                                                                     k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_deserialize_management_data() {
  if (!m_named_object_directory.deserialize(priv_make_core_file_name(m_base_dir_path,
                                                                     k_named_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.deserialize(priv_make_core_file_name(m_base_dir_path,
                                                                      k_unique_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.deserialize(priv_make_core_file_name(m_base_dir_path,
                                                                         k_anonymous_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.deserialize(priv_make_core_file_name(m_base_dir_path,
                                                                       k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

// ---------------------------------------- File operations ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_copy_data_store(const std::string &src_base_dir_path,
                                                       const std::string &dst_base_dir_path,
                                                       [[maybe_unused]] const bool overwrite) {
  const std::string src_datastore_dir_path = priv_make_top_dir_path(src_base_dir_path);
  if (!util::directory_exist(src_datastore_dir_path)) {
    logger::out(logger::level::critical, __FILE__, __LINE__,
                "Source directory does not exist: " + src_datastore_dir_path);
    return false;
  }

  if (!util::create_directory(dst_base_dir_path)) {
    logger::out(logger::level::critical, __FILE__, __LINE__,
                "Failed to create directory: " + dst_base_dir_path);
    return false;
  }

  const std::string dst_datastore_dir_path = priv_make_top_dir_path(dst_base_dir_path);

  assert(*(src_datastore_dir_path.end()) != '/');
  return util::clone_file(src_datastore_dir_path, dst_datastore_dir_path, true);
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_remove_data_store(const std::string &base_dir_path) {
  return util::remove_file(priv_make_top_dir_path(base_dir_path));
}

// ---------------------------------------- Management metadata ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_write_management_metadata(const std::string &base_dir_path,
                                                                      const json_store &json_root) {

  if (!util::ptree::write_json(json_root,
                               priv_make_core_file_name(base_dir_path, k_manager_metadata_file_name))) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to write management metadata");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_read_management_metadata(const std::string &base_dir_path,
                                                                     json_store *json_root) {
  if (!util::ptree::read_json(priv_make_core_file_name(base_dir_path, k_manager_metadata_file_name), json_root)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to read management metadata");
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
version_type manager_kernel<chnk_no, chnk_sz>::priv_get_version(const json_store &metadata_json) {
  version_type version;
  if (!util::ptree::get_value(metadata_json, k_manager_metadata_key_for_version, &version)) {
    return detail::k_error_version;
  }
  return version;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_set_version(json_store *metadata_json) {
  if (util::ptree::count(*metadata_json, k_manager_metadata_key_for_version) > 0) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Version information already exist");
    return false;
  }

  if (!util::ptree::add_value(k_manager_metadata_key_for_version, version_type(METALL_VERSION), metadata_json)) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string manager_kernel<chnk_no, chnk_sz>::priv_get_uuid(const json_store &metadata_json) {
  std::string uuid_string;
  if (!util::ptree::get_value(metadata_json, k_manager_metadata_key_for_uuid, &uuid_string)) {
    return "";
  }

  return uuid_string;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_set_uuid(json_store *metadata_json) {
  std::stringstream uuid_ss;
  uuid_ss << util::uuid(util::uuid_random_generator{}());
  if (!uuid_ss) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to convert UUID to std::string");
    return false;
  }

  if (util::ptree::count(*metadata_json, k_manager_metadata_key_for_uuid) > 0) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "UUID already exist");
    return false;
  }

  if (!util::ptree::add_value(k_manager_metadata_key_for_uuid, uuid_ss.str(), metadata_json)) {
    return false;
  }

  return true;
}


// ---------------------------------------- Description ---------------------------------------- //

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_read_description(const std::string &base_dir_path,
                                                             std::string *description) {
  const auto &file_name = priv_make_core_file_name(base_dir_path, k_description_file_name);

  try {
    std::ifstream ifs(file_name);
    if (!ifs.is_open()) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to open: " + file_name);
      return false;
    }

    if(!(ifs >> *description)) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to read data:" + file_name);
      return false;
    }

    ifs.close();
  } catch (const std::ios_base::failure& e) {
    logger::out(logger::level::error, __FILE__, __LINE__, std::string("Exception was thrown: ") + e.what());
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_write_description(const std::string &base_dir_path,
                                                              const std::string &description) {

  const auto &file_name = priv_make_core_file_name(base_dir_path, k_description_file_name);

  try {
    std::ofstream ofs(file_name);
    if (!ofs.is_open()) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to open: " + file_name);
      return false;
    }

    if(!(ofs << description)) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to write data:" + file_name);
      return false;
    }

    ofs.close();
  } catch (const std::ios_base::failure& e) {
    logger::out(logger::level::error, __FILE__, __LINE__, std::string("Exception was thrown: ") + e.what());
    return false;
  }

  return true;
}

} // namespace kernel
} // namespace metall

#endif //METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
