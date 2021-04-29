// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
#define METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP

#include <metall/kernel/manager_kernel_fwd.hpp>

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
      m_manager_metadata(nullptr)
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    , m_object_directories_mutex(nullptr)
#endif
{
  m_manager_metadata = std::make_unique<json_store>();

#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
  m_object_directories_mutex = std::make_unique<mutex_type>();
#endif
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
  if (offset == segment_memory_allocator::k_null_offset) {
    return nullptr;
  }
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
  if (offset == segment_memory_allocator::k_null_offset) {
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
    auto itr = m_unique_object_directory.find(gen_type_name<T>());
    if (itr != m_unique_object_directory.end()) {
      auto *const addr = reinterpret_cast<T *>(priv_to_address(itr->offset()));
      const auto length = itr->length();
      return std::make_pair(addr, length);
    }
  } else {
    auto itr = m_named_object_directory.find(name.get());
    if (itr != m_named_object_directory.end()) {
      auto *const addr = reinterpret_cast<T *>(priv_to_address(itr->offset()));
      const auto length = itr->length();
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
    return false; // Cannot destroy anoymous object by name
  }

  T *ptr = nullptr;
  size_type length = 0;

  {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    std::tie(ptr, length) = find<T>(name);
    if (!ptr) {
      return false; // This is not a critical error --- could have been destroyed by another thread already.
    }
    if (!priv_remove_attr_object_no_mutex(priv_to_offset(ptr))) {
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(priv_to_offset(ptr), length);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::destroy_ptr(const T *ptr) {
  assert(priv_initialized());
  if (m_segment_storage.read_only()) return false;

  size_type length = 0;
  {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    length = get_instance_length(ptr);
    if (length == 0) {
      return false; // This is not a critical error --- could have been destroyed by another thread already.
    }
    if (!priv_remove_attr_object_no_mutex(priv_to_offset(ptr))) {
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(priv_to_offset(ptr), length);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
const typename manager_kernel<chnk_no, chnk_sz>::char_type *
manager_kernel<chnk_no, chnk_sz>::get_instance_name(const T *ptr) const {

  auto nitr = m_named_object_directory.find(priv_to_offset(ptr));
  if (nitr != m_named_object_directory.end()) {
    return nitr->name().c_str();
  }

  auto uitr = m_unique_object_directory.find(priv_to_offset(ptr));
  if (uitr != m_unique_object_directory.end()) {
    return uitr->name().c_str();
  }

  return nullptr; // This is not error, anonymous object or non-constructed object
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
typename manager_kernel<chnk_no, chnk_sz>::instance_kind
manager_kernel<chnk_no, chnk_sz>::get_instance_kind(const T *ptr) const {
  if (m_named_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_kind::named_kind;
  }

  if (m_unique_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_kind::unique_kind;
  }

  if (m_anonymous_object_directory.count(priv_to_offset(ptr)) > 0) {
    return instance_kind::anonymous_kind;
  }

  logger::out(logger::level::error, __FILE__, __LINE__, "Invalid pointer");
  return instance_kind();
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_instance_length(const T *ptr) const {
  {
    auto itr = m_named_object_directory.find(priv_to_offset(ptr));
    if (itr != m_named_object_directory.end()) {
      assert(itr->length() > 0);
      return itr->length();
    }
  }

  {
    auto itr = m_unique_object_directory.find(priv_to_offset(ptr));
    if (itr != m_unique_object_directory.end()) {
      assert(itr->length() > 0);
      return itr->length();
    }
  }

  {
    auto itr = m_anonymous_object_directory.find(priv_to_offset(ptr));
    if (itr != m_anonymous_object_directory.end()) {
      assert(itr->length() > 0);
      return itr->length();
    }
  }

  return 0; // Won't treat as an error
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool
manager_kernel<chnk_no, chnk_sz>::is_instance_type(const void *const ptr) const {
  {
    auto itr = m_named_object_directory.find(priv_to_offset(ptr));
    if (itr != m_named_object_directory.end()) {
      return itr->type_id() == gen_type_id<T>();
    }
  }

  {
    auto itr = m_unique_object_directory.find(priv_to_offset(ptr));
    if (itr != m_unique_object_directory.end()) {
      return itr->type_id() == gen_type_id<T>();
    }
  }

  {
    auto itr = m_anonymous_object_directory.find(priv_to_offset(ptr));
    if (itr != m_anonymous_object_directory.end()) {
      return itr->type_id() == gen_type_id<T>();
    }
  }

  return false;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::get_instance_description(const T *ptr, std::string *description) const {
  {
    auto itr = m_named_object_directory.find(priv_to_offset(ptr));
    if (itr != m_named_object_directory.end()) {
      *description = itr->description();
      return true;
    }
  }

  {
    auto itr = m_unique_object_directory.find(priv_to_offset(ptr));
    if (itr != m_unique_object_directory.end()) {
      *description = itr->description();
      return true;
    }
  }

  {
    auto itr = m_anonymous_object_directory.find(priv_to_offset(ptr));
    if (itr != m_anonymous_object_directory.end()) {
      *description = itr->description();
      return true;
    }
  }

  return false;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::set_instance_description(const T *ptr, const std::string &description) {
  if (m_segment_storage.read_only()) return false;

  return (m_named_object_directory.set_description(m_named_object_directory.find(priv_to_offset(ptr)), description)
      || m_unique_object_directory.set_description(m_unique_object_directory.find(priv_to_offset(ptr)), description)
      || m_anonymous_object_directory.set_description(m_anonymous_object_directory.find(priv_to_offset(ptr)),
                                                      description));
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
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_num_anonymous_objects() const {
  return m_anonymous_object_directory.size();
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
typename manager_kernel<chnk_no, chnk_sz>::const_anonymous_iterator
manager_kernel<chnk_no, chnk_sz>::anonymous_begin() const {
  return m_anonymous_object_directory.begin();
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::const_anonymous_iterator
manager_kernel<chnk_no, chnk_sz>::anonymous_end() const {
  return m_anonymous_object_directory.end();
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
T *manager_kernel<chnk_no, chnk_sz>::generic_construct(char_ptr_holder_type name,
                                                       const size_type num,
                                                       const bool try2find,
                                                       [[maybe_unused]] const bool do_throw,
                                                       mdtl::in_place_interface &table) {
  assert(priv_initialized());
  return priv_generic_construct<T>(name, num, try2find, table);
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
typename manager_kernel<chnk_no, chnk_sz>::size_type
manager_kernel<chnk_no, chnk_sz>::get_segment_size() const {
  return m_segment_storage.size();
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::snapshot(const char *destination_base_dir_path, const bool clone, const int num_max_copy_threads) {
  return priv_snapshot(destination_base_dir_path, clone, num_max_copy_threads);
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::copy(const char *source_base_dir_path,
                                            const char *destination_base_dir_path,
                                            const bool clone,
                                            const int num_max_copy_threads) {
  return priv_copy_data_store(source_base_dir_path, destination_base_dir_path, clone, num_max_copy_threads);
}

template <typename chnk_no, std::size_t chnk_sz>
std::future<bool>
manager_kernel<chnk_no, chnk_sz>::copy_async(const char *source_dir_path,
                                             const char *destination_dir_path,
                                             const bool clone,
                                             const int num_max_copy_threads) {
  return std::async(std::launch::async, copy, source_dir_path, destination_dir_path, clone, num_max_copy_threads);
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
    std::stringstream ss;
    ss << "Cannot read management metadata in " << dir_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
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
    std::stringstream ss;
    ss << "Cannot read management metadata in " << dir_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return 0;
  }
  const auto version = priv_get_version(meta_data);
  return (version == ver_detail::k_error_version) ? 0 : version;
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

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::named_object_attr_accessor_type
manager_kernel<chnk_no, chnk_sz>::access_named_object_attribute(const std::string &base_dir_path) {
  return named_object_attr_accessor_type(priv_make_management_file_name(base_dir_path,
                                                                        k_named_object_directory_prefix));
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::unique_object_attr_accessor_type
manager_kernel<chnk_no, chnk_sz>::access_unique_object_attribute(const std::string &base_dir_path) {
  return unique_object_attr_accessor_type(priv_make_management_file_name(base_dir_path,
                                                                         k_unique_object_directory_prefix));
}

template <typename chnk_no, std::size_t chnk_sz>
typename manager_kernel<chnk_no, chnk_sz>::anonymous_object_attr_accessor_type
manager_kernel<chnk_no, chnk_sz>::access_anonymous_object_attribute(const std::string &base_dir_path) {
  return anonymous_object_attr_accessor_type(priv_make_management_file_name(base_dir_path,
                                                                            k_anonymous_object_directory_prefix));
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
manager_kernel<chnk_no, chnk_sz>::priv_make_management_dir_path(const std::string &base_dir_path) {
  return priv_make_top_dir_path(base_dir_path) + "/" + k_datastore_management_dir_name + "/";
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_management_file_name(const std::string &base_dir_path,
                                                                 const std::string &item_name) {
  return priv_make_management_dir_path(base_dir_path) + "/" + item_name;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string
manager_kernel<chnk_no, chnk_sz>::priv_make_segment_dir_path(const std::string &base_dir_path) {
  return priv_make_top_dir_path(base_dir_path) + "/" + k_datastore_segment_dir_name + "/";
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_init_datastore_directory(const std::string &base_dir_path) {
  // Create the base directory if needed
  if (!mdtl::create_directory(base_dir_path)) {
    std::string s("Failed to create directory: " + base_dir_path);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }

  // Remove existing directory to certainly create a new data store
  if (!remove(base_dir_path.c_str())) {
    std::string s("Failed to remove a directory: " + base_dir_path);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }

  // Create internal directories if needed
  if (!mdtl::create_directory(priv_make_management_dir_path(base_dir_path))) {
    std::string s("Failed to create directory: " + priv_make_management_dir_path(base_dir_path));
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (!mdtl::create_directory(priv_make_segment_dir_path(base_dir_path))) {
    std::string s("Failed to create directory: " + priv_make_segment_dir_path(base_dir_path));
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
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
  const auto system_page_size = mdtl::get_page_size();
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
  return mdtl::file_exist(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_mark_properly_closed(const std::string &base_dir_path) {
  return mdtl::create_file(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_unmark_properly_closed(const std::string &base_dir_path) {
  return mdtl::remove_file(priv_make_top_level_file_name(base_dir_path, k_properly_closed_mark_file_name));
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_reserve_vm_region(const size_type nbytes) {
  // Align the VM region to the page size to decrease the implementation cost of some features, such as
  // supporting Umap and aligned allocation
  const auto alignment = k_chunk_size;

  assert(alignment > 0);
  m_vm_region_size = mdtl::round_up(nbytes, alignment);
  m_vm_region = mdtl::reserve_aligned_vm_region(alignment, m_vm_region_size);
  if (!m_vm_region) {
    std::stringstream ss;
    ss << "Cannot reserve a VM region " << nbytes << " bytes";
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    m_vm_region_size = 0;
    return false;
  }
  assert(reinterpret_cast<uint64_t>(m_vm_region) % alignment == 0);

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_release_vm_region() {

  if (!mdtl::munmap(m_vm_region, m_vm_region_size, false)) {
    std::stringstream ss;
    ss << "Cannot release a VM region " << (uint64_t)m_vm_region << ", " << m_vm_region_size << " bytes.";
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
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

  if (mdtl::map_anonymous_write_mode(addr, k_segment_header_size, MAP_FIXED) != addr) {
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
  std::destroy_at(&m_segment_header);
  const auto ret = mdtl::munmap(m_segment_header, k_segment_header_size, false);
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
priv_generic_construct(char_ptr_holder_type name,
                       size_type length,
                       bool try2find,
                       mdtl::in_place_interface &table) {
  // Check overflow for security
  if (length > ((std::size_t)-1) / table.size) {
    return nullptr;
  }

  void *ptr = nullptr;
  try {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    if (!name.is_anonymous()) {
      auto *const found_addr = find<T>(name).first;
      if (found_addr) {
        if (try2find) {
          return found_addr;
        }
        return nullptr; // this is not a critical error always --- could have been allocated by another thread.
      }
    }

    ptr = allocate(length * sizeof(T));
    if (!ptr) return nullptr;

    const auto offset = priv_to_offset(ptr);
    if (!priv_register_attr_object_no_mutex<T>(name, offset, length)) {
      deallocate(ptr);
      return nullptr;
    }
  } catch (...) {
    logger::out(logger::level::critical,
                __FILE__,
                __LINE__,
                "Exception was thrown when finding or allocating an attribute object");
    return nullptr;
  }

  // To prevent memory leak, deallocates the memory when array_construct throws exception
  std::unique_ptr<void, std::function<void(void *)>> ptr_holder(ptr, [this](void *const ptr) {
    try {
      {
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
        lock_guard_type guard(*m_object_directories_mutex);
#endif
        priv_remove_attr_object_no_mutex(priv_to_offset(ptr));
      }
      deallocate(ptr);
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when cleaning up an object");
    }
  });

  // Constructs each object in the allocated memory
  // When one of objects of T in the array throws exception,
  // this function calls T's destructor for successfully constructed objects and rethrows the exception
  mdtl::array_construct(ptr, length, table);

  ptr_holder.release(); // release the pointer since the construction succeeded

  return static_cast<T *>(ptr);
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
bool manager_kernel<chnk_no, chnk_sz>::priv_register_attr_object_no_mutex(char_ptr_holder_type name,
                                                                          difference_type offset,
                                                                          size_type length) {
  if (name.is_anonymous()) {
    if (!m_anonymous_object_directory.insert("", offset, length, gen_type_id<T>())) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to insert an entry into the anonymous object table");
      return false;
    }
  } else if (name.is_unique()) {
    if (!m_unique_object_directory.insert(gen_type_name<T>(), offset, length, gen_type_id<T>())) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to insert an entry into the unique object table");
      return false;
    }
  } else {
    if (std::string(name.get()).empty()) {
      logger::out(logger::level::warning, __FILE__, __LINE__, "Empty name is invalid for named object");
      return false;
    }

    if (!m_named_object_directory.insert(name.get(), offset, length, gen_type_id<T>())) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to insert an entry into the named object table");
      return false;
    }
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_remove_attr_object_no_mutex(difference_type offset) {

  // As the instance kind of the object is not given,
  // just call the eranse functions in all tables to simplify implementation.
  if (!m_named_object_directory.erase(offset)
      && !m_unique_object_directory.erase(offset)
      && !m_anonymous_object_directory.erase(offset)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to erase an entry from object directories");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
template <typename T>
void manager_kernel<chnk_no, chnk_sz>::priv_destruct_and_free_memory(const difference_type offset,
                                                                     const size_type length) {
  auto *object = static_cast<T *>(priv_to_address(offset));
  // Destruct each object, can throw
  std::destroy(&object[0], &object[length]);
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

  if (!priv_read_management_metadata(base_dir_path, m_manager_metadata.get())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to read management metadata");
    return false;
  }

  if (!priv_check_version(*m_manager_metadata)) {
    std::stringstream ss;
    ss << "Invalid version — it was created by Metall v" << to_version_string(priv_get_version(*m_manager_metadata))
       << " (currently using v" << to_version_string(METALL_VERSION) << ")";
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  if (!priv_properly_closed(base_dir_path)) {
    logger::out(logger::level::critical, __FILE__, __LINE__,
                "Inconsistent data store — it was not closed properly and might have been collapsed.");
    return false;
  }

  m_base_dir_path = base_dir_path;

  const size_type existing_segment_size = segment_storage_type::get_size(priv_make_segment_dir_path(m_base_dir_path));
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

  if (!m_segment_storage.open(priv_make_segment_dir_path(m_base_dir_path),
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
    std::stringstream ss;
    ss << "Too large VM region size is requested " << vm_reserve_size << " byte.";
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  m_base_dir_path = base_dir_path;

  if (!priv_unmark_properly_closed(m_base_dir_path) || !priv_init_datastore_directory(base_dir_path)) {
    std::stringstream ss;
    ss << "Failed to initialize datastore directory under " << base_dir_path;
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  if (!priv_reserve_vm_region(vm_reserve_size)) {
    return false;
  }

  if (!priv_allocate_segment_header(m_vm_region)) {
    priv_release_vm_region();
    return false;
  }

  if (!m_segment_storage.create(priv_make_segment_dir_path(m_base_dir_path),
                                m_vm_region_size - k_segment_header_size,
                                static_cast<char *>(m_vm_region) + k_segment_header_size,
                                k_initial_segment_size)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Cannot create application data segment");
    priv_deallocate_segment_header();
    priv_release_vm_region();
    return false;
  }

  if (!priv_set_uuid(m_manager_metadata.get()) || !priv_set_version(m_manager_metadata.get())
      || !priv_write_management_metadata(m_base_dir_path, *m_manager_metadata)) {
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

  if (!m_named_object_directory.serialize(priv_make_management_file_name(m_base_dir_path,
                                                                         k_named_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.serialize(priv_make_management_file_name(m_base_dir_path,
                                                                          k_unique_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.serialize(priv_make_management_file_name(m_base_dir_path,
                                                                             k_anonymous_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to serialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.serialize(priv_make_management_file_name(m_base_dir_path,
                                                                           k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool
manager_kernel<chnk_no, chnk_sz>::priv_deserialize_management_data() {
  if (!m_named_object_directory.deserialize(priv_make_management_file_name(m_base_dir_path,
                                                                           k_named_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.deserialize(priv_make_management_file_name(m_base_dir_path,
                                                                            k_unique_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.deserialize(priv_make_management_file_name(m_base_dir_path,
                                                                               k_anonymous_object_directory_prefix).c_str())) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to deserialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.deserialize(priv_make_management_file_name(m_base_dir_path,
                                                                             k_segment_memory_allocator_prefix))) {
    return false;
  }

  return true;
}

// ---------------------------------------- snapshot ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_snapshot(const char *destination_base_dir_path,
                                                     const bool clone,
                                                     const int num_max_copy_threads) {
  assert(priv_initialized());
  m_segment_storage.sync(true);
  priv_serialize_management_data();

  const auto dst_top_dir = priv_make_top_dir_path(destination_base_dir_path);
  if (!mdtl::create_directory(dst_top_dir)) {
    std::stringstream ss;
    ss << "Failed to create a directory: " << dst_top_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Copy segment directory
  const auto src_seg_dir = priv_make_segment_dir_path(m_base_dir_path);
  const auto dst_seg_dir = priv_make_segment_dir_path(destination_base_dir_path);
  if (!mdtl::create_directory(dst_seg_dir)) {
    std::stringstream ss;
    ss << "Failed to create directory: " << dst_seg_dir;
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }
  if (!m_segment_storage.copy(src_seg_dir, dst_seg_dir, clone, num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_seg_dir << " to " << dst_seg_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Copy management dircotry
  const auto src_mng_dir = priv_make_management_dir_path(m_base_dir_path);
  const auto dst_mng_dir = priv_make_management_dir_path(destination_base_dir_path);
  if (!mdtl::create_directory(dst_mng_dir)) {
    std::stringstream ss;
    ss << "Failed to create directory: " << dst_mng_dir;
    logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }
  // Use a normal copy instead of reflink.
  // reflink might slow down if there are many reflink copied files.
  if (!mtlldetail::copy_files_in_directory_in_parallel(src_mng_dir, dst_mng_dir, num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_mng_dir << " to " << dst_mng_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Make a new management metadata
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

// ---------------------------------------- File operations ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_copy_data_store(const std::string &src_base_dir_path,
                                                            const std::string &dst_base_dir_path,
                                                            const bool use_clone,
                                                            const int num_max_copy_threads) {
  const std::string src_top_dir = priv_make_top_dir_path(src_base_dir_path);
  if (!mdtl::directory_exist(src_top_dir)) {
    std::string s("Source directory does not exist: " + src_top_dir);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (!mdtl::create_directory(priv_make_top_dir_path(dst_base_dir_path))) {
    std::string s("Failed to create directory: " + dst_base_dir_path);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }

  // Copy segment directory
  const auto src_seg_dir = priv_make_segment_dir_path(src_base_dir_path);
  const auto dst_seg_dir = priv_make_segment_dir_path(dst_base_dir_path);
  if (!mdtl::create_directory(dst_seg_dir)) {
    std::string s("Failed to create directory: " + dst_seg_dir);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }
  if (!segment_storage_type::copy(src_seg_dir, dst_seg_dir, use_clone, num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_seg_dir << " to " << dst_seg_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Copy management dircotry
  const auto src_mng_dir = priv_make_management_dir_path(src_base_dir_path);
  const auto dst_mng_dir = priv_make_management_dir_path(dst_base_dir_path);
  if (!mdtl::create_directory(dst_mng_dir)) {
    std::string s("Failed to create directory: " + dst_mng_dir);
    logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    return false;
  }
  // Use a normal copy instead of reflink.
  // reflink might slow down if there are many reflink copied files.
  if (!mtlldetail::copy_files_in_directory_in_parallel(src_mng_dir, dst_mng_dir, num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_mng_dir << " to " << dst_mng_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Finally, mark it as properly-closed
  if (!priv_mark_properly_closed(dst_base_dir_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed to create a properly closed mark");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_remove_data_store(const std::string &base_dir_path) {
  return mdtl::remove_file(priv_make_top_dir_path(base_dir_path));
}

// ---------------------------------------- Management metadata ---------------------------------------- //
template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_write_management_metadata(const std::string &base_dir_path,
                                                                      const json_store &json_root) {

  if (!mdtl::ptree::write_json(json_root,
                               priv_make_management_file_name(base_dir_path, k_manager_metadata_file_name))) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to write management metadata");
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_read_management_metadata(const std::string &base_dir_path,
                                                                     json_store *json_root) {
  if (!mdtl::ptree::read_json(priv_make_management_file_name(base_dir_path, k_manager_metadata_file_name), json_root)) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to read management metadata");
    return false;
  }
  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
version_type manager_kernel<chnk_no, chnk_sz>::priv_get_version(const json_store &metadata_json) {
  version_type version;
  if (!mdtl::ptree::get_value(metadata_json, k_manager_metadata_key_for_version, &version)) {
    return ver_detail::k_error_version;
  }
  return version;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_set_version(json_store *metadata_json) {
  if (mdtl::ptree::count(*metadata_json, k_manager_metadata_key_for_version) > 0) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Version information already exist");
    return false;
  }

  if (!mdtl::ptree::add_value(k_manager_metadata_key_for_version, version_type(METALL_VERSION), metadata_json)) {
    return false;
  }

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
std::string manager_kernel<chnk_no, chnk_sz>::priv_get_uuid(const json_store &metadata_json) {
  std::string uuid_string;
  if (!mdtl::ptree::get_value(metadata_json, k_manager_metadata_key_for_uuid, &uuid_string)) {
    return "";
  }

  return uuid_string;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_set_uuid(json_store *metadata_json) {
  std::stringstream uuid_ss;
  uuid_ss << mdtl::uuid(mdtl::uuid_random_generator{}());
  if (!uuid_ss) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to convert UUID to std::string");
    return false;
  }

  if (mdtl::ptree::count(*metadata_json, k_manager_metadata_key_for_uuid) > 0) {
    logger::out(logger::level::critical, __FILE__, __LINE__, "UUID already exist");
    return false;
  }

  if (!mdtl::ptree::add_value(k_manager_metadata_key_for_uuid, uuid_ss.str(), metadata_json)) {
    return false;
  }

  return true;
}


// ---------------------------------------- Description ---------------------------------------- //

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_read_description(const std::string &base_dir_path,
                                                             std::string *description) {
  const auto &file_name = priv_make_management_file_name(base_dir_path, k_description_file_name);

  if (!mdtl::file_exist(file_name)) {
    return false; // This is not an error
  }

  std::ifstream ifs(file_name);
  if (!ifs.is_open()) {
    std::string s("Failed to open: " + file_name);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  description->assign((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

  ifs.close();

  return true;
}

template <typename chnk_no, std::size_t chnk_sz>
bool manager_kernel<chnk_no, chnk_sz>::priv_write_description(const std::string &base_dir_path,
                                                              const std::string &description) {

  const auto &file_name = priv_make_management_file_name(base_dir_path, k_description_file_name);

  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    std::string s("Failed to open: " + file_name);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (!(ofs << description)) {
    std::string s("Failed to write data:" + file_name);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  ofs.close();

  return true;
}

} // namespace kernel
} // namespace metall

#endif //METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
