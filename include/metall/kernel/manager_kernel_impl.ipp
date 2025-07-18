// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
#define METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP

#include <metall/kernel/manager_kernel_fwd.hpp>

namespace metall::kernel {

// -------------------- //
// Constructor
// -------------------- //
template <typename st, typename sst, typename cn, std::size_t cs>
manager_kernel<st, sst, cn, cs>::manager_kernel()
    : m_segment_memory_allocator(&m_segment_storage) {
  m_manager_metadata = std::make_unique<json_store>();
  if (!m_manager_metadata) {
    return;
  }
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
  m_object_directories_mutex = std::make_unique<mutex_type>();
  if (!m_object_directories_mutex) {
    return;
  }
#endif
  m_good = priv_validate_runtime_configuration();
}

template <typename st, typename sst, typename cn, std::size_t cs>
manager_kernel<st, sst, cn, cs>::~manager_kernel() noexcept {
  close();
}

// -------------------- //
// Public methods
// -------------------- //
template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::create(const path_type &base_path,
                                             const size_type vm_reserve_size) {
  return m_good = priv_create(base_path, vm_reserve_size);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::open_read_only(
    const path_type &base_path) {
  return m_good = priv_open(base_path, true, 0);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::open(
    const path_type &base_path, const size_type vm_reserve_size_request) {
  return m_good = priv_open(base_path, false, vm_reserve_size_request);
}

template <typename st, typename sst, typename cn, std::size_t cs>
void manager_kernel<st, sst, cn, cs>::close() {
  if (m_segment_storage.is_open()) {
    priv_check_sanity();
    if (!m_segment_storage.read_only()) {
      priv_serialize_management_data();
      m_segment_storage.sync(true);
    }

    m_good = false;
    m_segment_storage.release();

    m_properly_closed_mark.close();
  }
}

template <typename st, typename sst, typename cn, std::size_t cs>
void manager_kernel<st, sst, cn, cs>::flush(const bool synchronous) {
  priv_check_sanity();
  m_segment_storage.sync(synchronous);
}

template <typename st, typename sst, typename cn, std::size_t cs>
void *manager_kernel<st, sst, cn, cs>::allocate(
    const manager_kernel<st, sst, cn, cs>::size_type nbytes) {
  priv_check_sanity();
  if (m_segment_storage.read_only()) return nullptr;

  const auto offset = m_segment_memory_allocator.allocate(nbytes);
  if (offset == segment_memory_allocator::k_null_offset) {
    return nullptr;
  }
  assert(offset >= 0);

  return priv_to_address(offset);
}

template <typename st, typename sst, typename cn, std::size_t cs>
void *manager_kernel<st, sst, cn, cs>::allocate_aligned(
    const manager_kernel<st, sst, cn, cs>::size_type nbytes,
    const manager_kernel<st, sst, cn, cs>::size_type alignment) {
  priv_check_sanity();
  if (m_segment_storage.read_only()) return nullptr;

  // This requirement could be removed, but it would need some work to do
  if (alignment > k_chunk_size) return nullptr;

  const auto offset =
      m_segment_memory_allocator.allocate_aligned(nbytes, alignment);
  if (offset == segment_memory_allocator::k_null_offset) {
    return nullptr;
  }
  assert(offset >= 0);

  assert((std::ptrdiff_t)m_segment_storage.get_segment() % alignment == 0);
  auto *addr = priv_to_address(offset);
  assert((uint64_t)addr % alignment == 0);
  return addr;
}

template <typename st, typename sst, typename cn, std::size_t cs>
void manager_kernel<st, sst, cn, cs>::deallocate(void *const addr) {
  priv_check_sanity();
  if (m_segment_storage.read_only()) return;
  if (!addr) return;
  m_segment_memory_allocator.deallocate(priv_to_offset(addr));
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::all_memory_deallocated() const {
  priv_check_sanity();
  return m_segment_memory_allocator.all_memory_deallocated();
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
std::pair<T *, typename manager_kernel<st, sst, cn, cs>::size_type>
manager_kernel<st, sst, cn, cs>::find(char_ptr_holder_type name) const {
  priv_check_sanity();

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

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::destroy(char_ptr_holder_type name) {
  priv_check_sanity();
  if (m_segment_storage.read_only()) return false;

  if (name.is_anonymous()) {
    return false;  // Cannot destroy anoymous object by name
  }

  T *ptr = nullptr;
  size_type length = 0;

  {
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    std::tie(ptr, length) = find<T>(name);
    if (!ptr) {
      return false;  // This is not a critical error --- could have been
                     // destroyed by another thread already.
    }
    if (!priv_remove_attr_object_no_mutex(priv_to_offset(ptr))) {
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(priv_to_offset(ptr), length);

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::destroy_ptr(const T *ptr) {
  priv_check_sanity();
  if (m_segment_storage.read_only()) return false;

  size_type length = 0;
  {
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    length = get_instance_length(ptr);
    if (length == 0) {
      return false;  // This is not a critical error --- could have been
                     // destroyed by another thread already.
    }
    if (!priv_remove_attr_object_no_mutex(priv_to_offset(ptr))) {
      return false;
    }
  }

  priv_destruct_and_free_memory<T>(priv_to_offset(ptr), length);

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
const typename manager_kernel<st, sst, cn, cs>::char_type *
manager_kernel<st, sst, cn, cs>::get_instance_name(const T *ptr) const {
  auto nitr = m_named_object_directory.find(priv_to_offset(ptr));
  if (nitr != m_named_object_directory.end()) {
    return nitr->name().c_str();
  }

  auto uitr = m_unique_object_directory.find(priv_to_offset(ptr));
  if (uitr != m_unique_object_directory.end()) {
    return uitr->name().c_str();
  }

  return nullptr;  // This is not error, anonymous object or non-constructed
                   // object
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
typename manager_kernel<st, sst, cn, cs>::instance_kind
manager_kernel<st, sst, cn, cs>::get_instance_kind(const T *ptr) const {
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

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
typename manager_kernel<st, sst, cn, cs>::size_type
manager_kernel<st, sst, cn, cs>::get_instance_length(const T *ptr) const {
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

  return 0;  // Won't treat as an error
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::is_instance_type(
    const void *const ptr) const {
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

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::get_instance_description(
    const T *ptr, std::string *description) const {
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

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::set_instance_description(
    const T *ptr, const std::string &description) {
  if (m_segment_storage.read_only()) return false;

  return (
      m_named_object_directory.set_description(
          m_named_object_directory.find(priv_to_offset(ptr)), description) ||
      m_unique_object_directory.set_description(
          m_unique_object_directory.find(priv_to_offset(ptr)), description) ||
      m_anonymous_object_directory.set_description(
          m_anonymous_object_directory.find(priv_to_offset(ptr)), description));
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::size_type
manager_kernel<st, sst, cn, cs>::get_num_named_objects() const {
  return m_named_object_directory.size();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::size_type
manager_kernel<st, sst, cn, cs>::get_num_unique_objects() const {
  return m_unique_object_directory.size();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::size_type
manager_kernel<st, sst, cn, cs>::get_num_anonymous_objects() const {
  return m_anonymous_object_directory.size();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_named_iterator
manager_kernel<st, sst, cn, cs>::named_begin() const {
  return m_named_object_directory.begin();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_named_iterator
manager_kernel<st, sst, cn, cs>::named_end() const {
  return m_named_object_directory.end();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_unique_iterator
manager_kernel<st, sst, cn, cs>::unique_begin() const {
  return m_unique_object_directory.begin();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_unique_iterator
manager_kernel<st, sst, cn, cs>::unique_end() const {
  return m_unique_object_directory.end();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_anonymous_iterator
manager_kernel<st, sst, cn, cs>::anonymous_begin() const {
  return m_anonymous_object_directory.begin();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::const_anonymous_iterator
manager_kernel<st, sst, cn, cs>::anonymous_end() const {
  return m_anonymous_object_directory.end();
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T, typename proxy>
T *manager_kernel<st, sst, cn, cs>::generic_construct(
    char_ptr_holder_type name, const size_type num, const bool try2find,
    [[maybe_unused]] const bool do_throw, proxy &pr) {
  priv_check_sanity();
  return priv_generic_construct<T>(name, num, try2find, pr);
}

template <typename st, typename sst, typename cn, std::size_t cs>
const typename manager_kernel<st, sst, cn, cs>::segment_header_type &
manager_kernel<st, sst, cn, cs>::get_segment_header() const {
  return m_segment_storage.get_segment_header();
}

template <typename st, typename sst, typename cn, std::size_t cs>
const void *manager_kernel<st, sst, cn, cs>::get_segment() const {
  return m_segment_storage.get_segment();
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::size_type
manager_kernel<st, sst, cn, cs>::get_segment_size() const {
  return m_segment_storage.size();
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::read_only() const {
  return m_segment_storage.read_only();
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::snapshot(
    const path_type &destination_base_path, const bool clone,
    const int num_max_copy_threads) {
  return priv_snapshot(destination_base_path, clone, num_max_copy_threads);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::copy(
    const path_type &source_base_path, const path_type &destination_base_path,
    const bool clone, const int num_max_copy_threads) {
  return priv_copy_data_store(source_base_path, destination_base_path, clone,
                              num_max_copy_threads);
}

template <typename st, typename sst, typename cn, std::size_t cs>
std::future<bool> manager_kernel<st, sst, cn, cs>::copy_async(
    const path_type &source_base_path, const path_type &destination_base_path,
    const bool clone, const int num_max_copy_threads) {
  return std::async(std::launch::async, copy, source_base_path,
                    destination_base_path, clone, num_max_copy_threads);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::remove(const path_type &base_path) {
  return priv_remove_data_store(base_path);
}

template <typename st, typename sst, typename cn, std::size_t cs>
std::future<bool> manager_kernel<st, sst, cn, cs>::remove_async(
    const path_type &base_path) {
  return std::async(std::launch::async, remove, base_path);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::consistent(const path_type &base_path) {
  return priv_consistent(base_path);
}

template <typename st, typename sst, typename cn, std::size_t cs>
std::string manager_kernel<st, sst, cn, cs>::get_uuid() const {
  return self_type::get_uuid(m_base_path);
}

template <typename st, typename sst, typename cn, std::size_t cs>
std::string manager_kernel<st, sst, cn, cs>::get_uuid(
    const path_type &base_path) {
  json_store meta_data;
  if (!priv_read_management_metadata(base_path, &meta_data)) {
    std::stringstream ss;
    ss << "Cannot read management metadata in " << base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return "";
  }
  return priv_get_uuid(meta_data);
}

template <typename st, typename sst, typename cn, std::size_t cs>
version_type manager_kernel<st, sst, cn, cs>::get_version() const {
  return self_type::get_version(m_base_path);
}

template <typename st, typename sst, typename cn, std::size_t cs>
version_type manager_kernel<st, sst, cn, cs>::get_version(
    const path_type &base_path) {
  json_store meta_data;
  if (!priv_read_management_metadata(base_path, &meta_data)) {
    std::stringstream ss;
    ss << "Cannot read management metadata in " << base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return 0;
  }
  const auto version = priv_get_version(meta_data);
  return (version == ver_detail::k_error_version) ? 0 : version;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::get_description(
    const path_type &base_path, std::string *description) {
  return priv_read_description(base_path, description);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::get_description(
    std::string *description) const {
  return priv_read_description(m_base_path, description);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::set_description(
    const path_type &base_path, const std::string &description) {
  return priv_write_description(base_path, description);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::set_description(
    const std::string &description) {
  return set_description(m_base_path, description);
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::named_object_attr_accessor_type
manager_kernel<st, sst, cn, cs>::access_named_object_attribute(
    const path_type &base_path) {
  return named_object_attr_accessor_type(storage::get_path(
      base_path, {k_management_dir_name, k_named_object_directory_prefix}));
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::unique_object_attr_accessor_type
manager_kernel<st, sst, cn, cs>::access_unique_object_attribute(
    const path_type &base_path) {
  return unique_object_attr_accessor_type(storage::get_path(
      base_path, {k_management_dir_name, k_unique_object_directory_prefix}));
}

template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::anonymous_object_attr_accessor_type
manager_kernel<st, sst, cn, cs>::access_anonymous_object_attribute(
    const path_type &base_path) {
  return anonymous_object_attr_accessor_type(storage::get_path(
      base_path, {k_management_dir_name, k_anonymous_object_directory_prefix}));
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::good() const noexcept {
  return m_good;
}

// -------------------- //
// Private methods
// -------------------- //
template <typename st, typename sst, typename cn, std::size_t cs>
typename manager_kernel<st, sst, cn, cs>::difference_type
manager_kernel<st, sst, cn, cs>::priv_to_offset(const void *const ptr) const {
  return static_cast<char *>(const_cast<void *>(ptr)) -
         static_cast<char *>(m_segment_storage.get_segment());
}

template <typename st, typename sst, typename cn, std::size_t cs>
void *manager_kernel<st, sst, cn, cs>::priv_to_address(
    const difference_type offset) const {
  return static_cast<char *>(m_segment_storage.get_segment()) + offset;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_create_datastore_directory(
    const path_type &base_path) {
  if (!storage::create(base_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to initialize the data store directory");
    return false;
  }

  // Create internal directories if needed
  if (!mdtl::create_directory(
          storage::get_path(base_path, k_management_dir_name))) {
    std::string s("Failed to create directory: " +
                  storage::get_path(base_path, k_management_dir_name).string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
void manager_kernel<st, sst, cn, cs>::priv_check_sanity() const {
  assert(m_good);
  assert(!m_base_path.empty());
  assert(m_segment_storage.check_sanity());
  // TODO: add sanity check functions in other classes
  assert(m_manager_metadata);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_validate_runtime_configuration()
    const {
  const auto system_page_size = mdtl::get_page_size();
  if (system_page_size <= 0) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to get the system page size");
    return false;
  }

  if (k_chunk_size % system_page_size != 0) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "The chunk size must be a multiple of the system page size");
    return false;
  }

  if (m_segment_storage.page_size() > k_chunk_size) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "The page size of the segment storage must be equal or smaller "
                "than the chunk size");
    return false;
  }

  if (m_segment_storage.page_size() % system_page_size != 0) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "The page size of the segment storage must be a multiple of "
                "the system page size");
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_consistent(
    const path_type &base_path) {
  json_store metadata;
  return priv_properly_closed(base_path) &&
         (priv_read_management_metadata(base_path, &metadata) &&
          priv_check_version(metadata));
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_check_version(
    const json_store &metadata_json) {
  return priv_get_version(metadata_json) == version_type(METALL_VERSION);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_properly_closed(
    const path_type &base_path) {
  return mdtl::file_exist(
      storage::get_path(base_path, k_properly_closed_mark_file_name));
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_mark_properly_closed(
    const path_type &base_path) {
  return mdtl::create_file(
      storage::get_path(base_path, k_properly_closed_mark_file_name));
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T, typename proxy>
T *manager_kernel<st, sst, cn, cs>::priv_generic_construct(
    char_ptr_holder_type name, size_type length, bool try2find, proxy &pr) {
  void *ptr = nullptr;
  try {
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
    lock_guard_type guard(*m_object_directories_mutex);
#endif

    if (!name.is_anonymous()) {
      auto *const found_addr = find<T>(name).first;
      if (found_addr) {
        if (try2find) {
          return found_addr;
        }
        return nullptr;  // this is not a critical error always --- could have
                         // been allocated by another thread.
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
    logger::out(
        logger::level::error, __FILE__, __LINE__,
        "Exception was thrown when finding or allocating an attribute object");
    return nullptr;
  }

  // To prevent memory leak, deallocates the memory when the array construction
  // below throws exception
  std::unique_ptr<void, std::function<void(void *)>> ptr_holder(
      ptr, [this](void *const ptr) {
        try {
          {
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
            lock_guard_type guard(*m_object_directories_mutex);
#endif
            priv_remove_attr_object_no_mutex(priv_to_offset(ptr));
          }
          deallocate(ptr);
        } catch (...) {
          logger::out(logger::level::error, __FILE__, __LINE__,
                      "Exception was thrown when cleaning up an object");
        }
      });

#if BOOST_VERSION >= 108500
  pr.construct_n(ptr, length);
#else
  // Constructs each object in the allocated memory
  // When one of objects of T in the array throws exception,
  // this function calls T's destructor for successfully constructed objects and
  // rethrows the exception
  std::size_t constructed = 0;
  try {
    pr.construct_n(ptr, length, constructed);
  } catch (...) {
    std::size_t destroyed = 0;
    pr.destroy_n(ptr, constructed, destroyed);
    throw;
  }
#endif
  ptr_holder.release();  // release the pointer since the construction succeeded

  return static_cast<T *>(ptr);
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
bool manager_kernel<st, sst, cn, cs>::priv_register_attr_object_no_mutex(
    char_ptr_holder_type name, difference_type offset, size_type length) {
  if (name.is_anonymous()) {
    if (!m_anonymous_object_directory.insert("", offset, length,
                                             gen_type_id<T>())) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to insert an entry into the anonymous object table");
      return false;
    }
  } else if (name.is_unique()) {
    if (!m_unique_object_directory.insert(gen_type_name<T>(), offset, length,
                                          gen_type_id<T>())) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to insert an entry into the unique object table");
      return false;
    }
  } else {
    if (std::string(name.get()).empty()) {
      logger::out(logger::level::warning, __FILE__, __LINE__,
                  "Empty name is invalid for named object");
      return false;
    }

    if (!m_named_object_directory.insert(name.get(), offset, length,
                                         gen_type_id<T>())) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to insert an entry into the named object table");
      return false;
    }
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_remove_attr_object_no_mutex(
    difference_type offset) {
  // As the instance kind of the object is not given,
  // just call the eranse functions in all tables to simplify implementation.
  if (!m_named_object_directory.erase(offset) &&
      !m_unique_object_directory.erase(offset) &&
      !m_anonymous_object_directory.erase(offset)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to erase an entry from object directories");
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename T>
void manager_kernel<st, sst, cn, cs>::priv_destruct_and_free_memory(
    const difference_type offset, const size_type length) {
  auto *object = static_cast<T *>(priv_to_address(offset));
  // Destruct each object, can throw
  std::destroy(&object[0], &object[length]);
  // Finally, deallocate the memory
  m_segment_memory_allocator.deallocate(offset);
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_open(
    const path_type &base_path, const bool read_only,
    const size_type vm_reserve_size_request) {
  if (!priv_validate_runtime_configuration()) {
    return false;
  }

  if (!priv_read_management_metadata(base_path, m_manager_metadata.get())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to read management metadata");
    return false;
  }

  if (!priv_check_version(*m_manager_metadata)) {
    std::stringstream ss;
    ss << "Invalid version — it was created by Metall v"
       << to_version_string(priv_get_version(*m_manager_metadata))
       << " (currently using v" << to_version_string(METALL_VERSION) << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  if (!m_properly_closed_mark.open(storage::get_path(base_path, k_properly_closed_mark_file_name),
                                   read_only)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Unable to open data store — either it is already open, or it was not "
                "closed properly and might have been collapsed.");
    return false;
  }

  m_base_path = base_path;

  if (!m_segment_storage.open(m_base_path, vm_reserve_size_request,
                              read_only)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to open the application data segment");
    return false;
  }
  m_segment_storage.get_segment_header().manager_kernel_address = this;

  if (!priv_deserialize_management_data()) {
    m_segment_storage.release();
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_create(
    const path_type &base_path, const size_type vm_reserve_size) {
  if (!priv_validate_runtime_configuration()) {
    return false;
  }

  if (vm_reserve_size > k_max_segment_size) {
    std::stringstream ss;
    ss << "Too large VM region size is requested " << vm_reserve_size
       << " byte.";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  if (!priv_create_datastore_directory(base_path)) {
    std::stringstream ss;
    ss << "Failed to initialize datastore under " << base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  if (!m_properly_closed_mark.create(storage::get_path(base_path, k_properly_closed_mark_file_name))) {
    std::stringstream ss;
    ss << "Failed to remove a closed mark under " << base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  m_base_path = base_path;

  if (!m_segment_storage.create(m_base_path, vm_reserve_size)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Cannot create an application data segment");
    return false;
  }
  m_segment_storage.get_segment_header().manager_kernel_address = this;

  if (!priv_set_uuid(m_manager_metadata.get()) ||
      !priv_set_version(m_manager_metadata.get()) ||
      !priv_write_management_metadata(m_base_path, *m_manager_metadata)) {
    m_segment_storage.release();
    return false;
  }

  return true;
}

// ---------- For serializing/deserializing ---------- //
template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_serialize_management_data() {
  priv_check_sanity();

  if (m_segment_storage.read_only()) return true;

  if (!m_named_object_directory.serialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_named_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to serialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.serialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_unique_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to serialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.serialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_anonymous_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to serialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.serialize(storage::get_path(
          m_base_path,
          {k_management_dir_name, k_segment_memory_allocator_prefix}))) {
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_deserialize_management_data() {
  if (!m_named_object_directory.deserialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_named_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to deserialize named object directory");
    return false;
  }

  if (!m_unique_object_directory.deserialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_unique_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to deserialize unique object directory");
    return false;
  }

  if (!m_anonymous_object_directory.deserialize(
          storage::get_path(m_base_path, {k_management_dir_name,
                                          k_anonymous_object_directory_prefix})
              .c_str())) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to deserialize anonymous object directory");
    return false;
  }

  if (!m_segment_memory_allocator.deserialize(storage::get_path(
          m_base_path,
          {k_management_dir_name, k_segment_memory_allocator_prefix}))) {
    return false;
  }

  return true;
}

// ---------- snapshot ---------- //
template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_snapshot(
    const path_type &destination_base_path, const bool clone,
    const int num_max_copy_threads) {
  priv_check_sanity();
  priv_serialize_management_data();

  if (!priv_create_datastore_directory(destination_base_path)) {
    std::stringstream ss;
    ss << "Failed to init the destination: " << destination_base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Copy segment directory
  if (!m_segment_storage.snapshot(destination_base_path, clone,
                                  num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << m_base_path << " to " << destination_base_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Copy management directory
  const auto src_mng_dir =
      storage::get_path(m_base_path, k_management_dir_name);
  const auto dst_mng_dir =
      storage::get_path(destination_base_path, k_management_dir_name);
  if (!mdtl::create_directory(dst_mng_dir)) {
    std::stringstream ss;
    ss << "Failed to create directory: " << dst_mng_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }
  // Use a normal copy instead of reflink.
  // reflink might slow down if there are many reflink copied files.
  if (!mtlldetail::copy_files_in_directory_in_parallel(src_mng_dir, dst_mng_dir,
                                                       num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_mng_dir << " to " << dst_mng_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Make a new management metadata
  json_store meta_data;
  if (!priv_set_uuid(&meta_data)) return false;
  if (!priv_set_version(&meta_data)) return false;
  if (!priv_write_management_metadata(destination_base_path, meta_data))
    return false;

  // Finally, mark it as properly-closed
  if (!priv_mark_properly_closed(destination_base_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to create a properly closed mark");
    return false;
  }

  return true;
}

// ---------- File operations ---------- //
template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_copy_data_store(
    const path_type &src_base_path, const path_type &dst_base_path,
    const bool use_clone, const int num_max_copy_threads) {
  if (!consistent(src_base_path)) {
    std::string s("Source directory is not consistent: " +
                  src_base_path.string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (!storage::create(dst_base_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to initialize the datastore directory");
    return false;
  }

  // Copy segment directory
  segment_storage::copy(src_base_path, dst_base_path, use_clone,
                        num_max_copy_threads);

  // Copy management directory
  const auto src_mng_dir =
      storage::get_path(src_base_path, k_management_dir_name);
  const auto dst_mng_dir =
      storage::get_path(dst_base_path, k_management_dir_name);
  if (!mdtl::create_directory(dst_mng_dir)) {
    std::string s("Failed to create directory: " + dst_mng_dir.string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }
  // Use a normal copy instead of reflink.
  // reflink might slow down if there are many reflink copied files.
  if (!mtlldetail::copy_files_in_directory_in_parallel(src_mng_dir, dst_mng_dir,
                                                       num_max_copy_threads)) {
    std::stringstream ss;
    ss << "Failed to copy " << src_mng_dir << " to " << dst_mng_dir;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }

  // Finally, mark it as properly-closed
  if (!priv_mark_properly_closed(dst_base_path)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to create a properly closed mark");
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_remove_data_store(
    const path_type &base_path) {
  return storage::remove(base_path);
}

// ---------- Management metadata ---------- //
template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_write_management_metadata(
    const path_type &base_path, const json_store &json_root) {
  if (!mdtl::ptree::write_json(
          json_root,
          storage::get_path(base_path, {k_management_dir_name,
                                        k_manager_metadata_file_name}))) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to write management metadata");
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_read_management_metadata(
    const path_type &base_path, json_store *json_root) {
  if (!mdtl::ptree::read_json(
          storage::get_path(
              base_path, {k_management_dir_name, k_manager_metadata_file_name}),
          json_root)) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to read management metadata");
    return false;
  }
  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
version_type manager_kernel<st, sst, cn, cs>::priv_get_version(
    const json_store &metadata_json) {
  version_type version;
  if (!mdtl::ptree::get_value(metadata_json, k_manager_metadata_key_for_version,
                              &version)) {
    return ver_detail::k_error_version;
  }
  return version;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_set_version(
    json_store *metadata_json) {
  if (mdtl::ptree::count(*metadata_json, k_manager_metadata_key_for_version) >
      0) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Version information already exist");
    return false;
  }

  if (!mdtl::ptree::add_value(k_manager_metadata_key_for_version,
                              version_type(METALL_VERSION), metadata_json)) {
    return false;
  }

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
std::string manager_kernel<st, sst, cn, cs>::priv_get_uuid(
    const json_store &metadata_json) {
  std::string uuid_string;
  if (!mdtl::ptree::get_value(metadata_json, k_manager_metadata_key_for_uuid,
                              &uuid_string)) {
    return "";
  }

  return uuid_string;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_set_uuid(json_store *metadata_json) {
  std::stringstream uuid_ss;
  uuid_ss << mdtl::uuid(mdtl::uuid_random_generator{}());
  if (!uuid_ss) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to convert UUID to std::string");
    return false;
  }

  if (mdtl::ptree::count(*metadata_json, k_manager_metadata_key_for_uuid) > 0) {
    logger::out(logger::level::error, __FILE__, __LINE__, "UUID already exist");
    return false;
  }

  if (!mdtl::ptree::add_value(k_manager_metadata_key_for_uuid, uuid_ss.str(),
                              metadata_json)) {
    return false;
  }

  return true;
}

// ---------- Description ---------- //

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_read_description(
    const path_type &base_path, std::string *description) {
  const auto &file_name = storage::get_path(
      base_path, {k_management_dir_name, k_description_file_name});

  if (!mdtl::file_exist(file_name)) {
    return false;  // This is not an error
  }

  std::ifstream ifs(file_name);
  if (!ifs.is_open()) {
    std::string s("Failed to open: " + file_name.string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  description->assign((std::istreambuf_iterator<char>(ifs)),
                      std::istreambuf_iterator<char>());

  ifs.close();

  return true;
}

template <typename st, typename sst, typename cn, std::size_t cs>
bool manager_kernel<st, sst, cn, cs>::priv_write_description(
    const path_type &base_path, const std::string &description) {
  const auto &file_name = storage::get_path(
      base_path, {k_management_dir_name, k_description_file_name});

  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    std::string s("Failed to open: " + file_name.string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (!(ofs << description)) {
    std::string s("Failed to write data:" + file_name.string());
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  ofs.close();

  return true;
}

}  // namespace metall::kernel

#endif  // METALL_DETAIL_KERNEL_MANAGER_KERNEL_IMPL_IPP
