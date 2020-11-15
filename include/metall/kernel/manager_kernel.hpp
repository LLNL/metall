// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_MANAGER_KERNEL_HPP
#define METALL_KERNEL_MANAGER_KERNEL_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <vector>
#include <map>
#include <sstream>

#include <metall/logger.hpp>
#include <metall/offset_ptr.hpp>
#include <metall/version.hpp>
#include <metall/kernel/manager_kernel_fwd.hpp>
#include <metall/kernel/manager_kernel_defs.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/segment_allocator.hpp>
#include <metall/kernel/named_object_directory.hpp>
#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/in_place_interface.hpp>
#include <metall/detail/utility/array_construct.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/file_clone.hpp>
#include <metall/detail/utility/char_ptr_holder.hpp>
#include <metall/detail/utility/soft_dirty_page.hpp>
#include <metall/detail/utility/uuid.hpp>
#include <metall/detail/utility/ptree.hpp>

#ifdef METALL_USE_UMAP
#include <metall/kernel/segment_storage/umap_sparse_segment_storage.hpp>
#else
#include <metall/kernel/segment_storage/mmap_segment_storage.hpp>
#endif

#define ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL 1
#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
#include <metall/detail/utility/mutex.hpp>
#endif

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <typename _chunk_no_type, std::size_t _chunk_size>
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

  using chunk_no_type = _chunk_no_type;
  static constexpr size_type k_chunk_size = _chunk_size;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = manager_kernel<_chunk_no_type, _chunk_size>;
  static constexpr const char *k_datastore_top_dir_name = "metall_datastore";
  static constexpr const char *k_datastore_core_dir_name = "core";

  // For segment
  static constexpr size_type k_default_vm_reserve_size = METALL_DEFAULT_VM_RESERVE_SIZE;
  static_assert(k_chunk_size <= k_default_vm_reserve_size, "Chunk size must be <= k_default_vm_reserve_size");

  static constexpr size_type k_max_segment_size = METALL_MAX_SEGMENT_SIZE;
  static_assert(k_default_vm_reserve_size <= k_max_segment_size,
                "k_default_vm_reserve_size must be <= k_max_segment_size");

  static constexpr size_type k_initial_segment_size = METALL_INITIAL_SEGMENT_SIZE;
  static_assert(k_initial_segment_size <= k_default_vm_reserve_size,
                "k_initial_segment_size must be <= k_default_vm_reserve_size");
  static_assert(k_chunk_size <= k_initial_segment_size, "Chunk size must be <= k_initial_segment_size");

  static constexpr const char *k_segment_prefix = "segment";

  using segment_header_type = segment_header;
  static constexpr size_type k_segment_header_size = util::round_up(sizeof(segment_header_type), k_chunk_size);

  using segment_storage_type =
#ifdef METALL_USE_UMAP
  umap_sparse_segment_storage<difference_type, size_type>;
#else
  mmap_segment_storage<difference_type, size_type>;
#endif

  // For actual memory allocation layer
  static constexpr const char *k_segment_memory_allocator_prefix = "segment_memory_allocator";
  using segment_memory_allocator = segment_allocator<chunk_no_type, size_type, difference_type,
                                                     k_chunk_size, k_max_segment_size,
                                                     segment_storage_type>;

  // For named object directory
  using named_object_directory_type = named_object_directory<difference_type, size_type>;
  static constexpr const char *k_named_object_directory_prefix = "named_object_directory";

  static constexpr const char *k_properly_closed_mark_file_name = "properly_closed_mark";

  // For manager metadata data
  static constexpr const char *k_manager_metadata_file_name = "manager_metadata";
  static constexpr const char *k_manager_metadata_key_for_version = "version";
  static constexpr const char *k_manager_metadata_key_for_uuid = "uuid";
  using json_store = metall::detail::utility::ptree::node_type;

#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  manager_kernel();
  ~manager_kernel() noexcept;

  manager_kernel(const manager_kernel &) = delete;
  manager_kernel &operator=(const manager_kernel &) = delete;

  manager_kernel(manager_kernel &&) noexcept = default;
  manager_kernel &operator=(manager_kernel &&) noexcept = default;

 public:
  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Creates a new datastore
  /// Expect to be called by a single thread
  /// \param base_dir_path
  /// \param vm_reserve_size
  /// \return Returns true if success; otherwise, returns false
  bool create(const char *base_dir_path, size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Opens an existing datastore
  /// Expect to be called by a single thread
  /// \param base_dir_path
  /// \param vm_reserve_size
  /// \return Returns true if success; otherwise, returns false
  bool open(const char *base_dir_path, size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Opens an existing datastore with read only
  /// Expect to be called by a single thread
  /// \param base_dir_path
  /// \return Returns true if success; otherwise, returns false
  bool open_read_only(const char *base_dir_path);

  /// \brief Expect to be called by a single thread
  void close();

  /// \brief Flush data to persistent memory
  /// \param synchronous If true, performs synchronous operation;
  /// otherwise, performs asynchronous operation.
  void flush(bool synchronous);

  /// \brief Allocates memory space
  /// \param nbytes
  /// \return
  void *allocate(size_type nbytes);

  /// \brief Allocate nbytes bytes of uninitialized storage whose alignment is specified by alignment.
  /// \param nbytes A size to allocate. Must be a multiple of alignment.
  /// \param alignment An alignment requirement.
  /// Alignment must be a power of two and satisfy [min allocation size, chunk size].
  /// \return On success, returns the pointer to the beginning of newly allocated memory.
  /// Returns nullptr, if the given arguments do not satisfy the requirements above.
  void *allocate_aligned(size_type nbytes, size_type alignment);

  /// \brief Deallocates
  /// \param addr
  void deallocate(void *addr);

  /// \brief Finds an already constructed object
  /// \tparam T
  /// \param name
  /// \return
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) const;

  /// \brief Destroy an already constructed object
  /// \tparam T
  /// \param name
  /// \return
  template <typename T>
  bool destroy(char_ptr_holder_type name);

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
                       size_type num,
                       bool try2find,
                       bool dothrow,
                       util::in_place_interface &table);

  /// \brief Get the address of the segment header
  /// \return Returns the address of the segment header
  segment_header_type *get_segment_header() const;

  /// \brief Takes a snapshot. The snapshot has a different UUID.
  /// \param destination_dir_path Destination path
  /// \return If succeeded, returns True; other false
  bool snapshot(const char *destination_dir_path);

  /// \brief Copies a data store synchronously, keeping the same UUID.
  /// \param source_dir_path Destination path
  /// \param destination_dir_path
  /// \return If succeeded, returns True; other false
  static bool copy(const char *source_dir_path, const char *destination_dir_path);

  /// \brief Copies a data store asynchronously, keeping the same UUID.
  /// \param source_dir_path Destination path
  /// \param destination_dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> copy_async(const char *source_dir_path, const char *destination_dir_path);

  /// \brief Remove a data store synchronously
  /// \param base_dir_path
  /// \return If succeeded, returns True; other false
  static bool remove(const char *base_dir_path);

  /// \brief Remove a data store asynchronously
  /// \param base_dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> remove_async(const char *base_dir_path);

  /// \brief Check if the backing data store is consistent,
  /// i.e. it was closed properly.
  /// \param dir_path
  /// \return Return true if it is consistent; otherwise, returns false.
  static bool consistent(const char *dir_path);

  /// \brief Returns the UUID of the backing data store.
  /// \return Returns UUID in std::string; returns an empty string on error.
  std::string get_uuid() const;

  /// \brief Returns the UUID of the backing data store.
  /// \param dir_path Path to a data store.
  /// \return Returns UUID in std::string; returns an empty string on error.
  static std::string get_uuid(const char *dir_path);

  /// \brief Gets the version number of the backing data store.
  /// \return Returns a version number; returns 0 on error.
  version_type get_version() const;

  /// \brief Gets the version number of the backing data store.
  /// \param dir_path Path to a data store.
  /// \return Returns a version number; returns 0 on error.
  static version_type get_version(const char *dir_path);

  /// \brief Show some profile information
  /// \tparam out_stream_type
  /// \param log_out
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) const;

 private:
  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //

  // Directory structure:
  // base_dir_path/ <- this path is given by user
  //  top_dir/
  //    some top-level file
  //    core_dir/
  //      many core files
  //      many directories
  static std::string priv_make_top_dir_path(const std::string &base_dir_path);
  static std::string priv_make_top_level_file_name(const std::string &base_dir_path, const std::string &item_name);
  static std::string priv_make_core_dir_path(const std::string &base_dir_path);
  static std::string priv_make_core_file_name(const std::string &base_dir_path, const std::string &item_name);
  static bool priv_init_datastore_directory(const std::string &base_dir_path);

  bool priv_initialized() const;
  bool priv_validate_runtime_configuration() const;

  static bool priv_consistent(const std::string &base_dir_path);
  static bool priv_check_version(const json_store& metadata_json);
  static bool priv_properly_closed(const std::string &base_dir_path);
  static bool priv_mark_properly_closed(const std::string &base_dir_path);
  static bool priv_unmark_properly_closed(const std::string &base_dir_path);

  template <typename T>
  T *priv_generic_named_construct(const char_type *name,
                                  size_type num,
                                  bool try2find,
                                  bool dothrow, // TODO implement 'dothrow'
                                  util::in_place_interface &table);

  // ---------------------------------------- For segment ---------------------------------------- //
  bool priv_reserve_vm_region(size_type nbytes);
  bool priv_release_vm_region();
  bool priv_allocate_segment_header(void *addr);
  bool priv_deallocate_segment_header();

  bool priv_open(const char *base_dir_path, const bool read_only, const size_type vm_reserve_size_request = 0);
  bool priv_create(const char *base_dir_path, const size_type vm_reserve_size);

  // ---------------------------------------- For serializing/deserializing ---------------------------------------- //
  bool priv_serialize_management_data();
  bool priv_deserialize_management_data();

  // ---------------------------------------- File operations ---------------------------------------- //
  /// \brief Copies all backing files using reflink if possible
  static bool priv_copy_data_store(const std::string &src_dir_path, const std::string &dst_dir_path, bool overwrite);

  /// \brief Removes all backing files
  static bool priv_remove_data_store(const std::string &dir_path);

  // ---------------------------------------- Management metadata ---------------------------------------- //
  static bool priv_read_management_metadata(const std::string &base_dir_path, json_store* json_root);
  static bool priv_write_management_metadata(const std::string &base_dir_path, const json_store& json_root);

  static version_type priv_get_version(const json_store& metadata_json);
  static bool priv_set_version(json_store* metadata_json);

  static bool priv_set_uuid(json_store* metadata_json);
  static std::string priv_get_uuid(const json_store& metadata_json);

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  std::string m_base_dir_path;
  size_type m_vm_region_size;
  void *m_vm_region;
  segment_header_type *m_segment_header;
  named_object_directory_type m_named_object_directory;
  segment_storage_type m_segment_storage;
  segment_memory_allocator m_segment_memory_allocator;
  json_store m_manager_metadata;

#if ENABLE_MUTEX_IN_METALL_MANAGER_KERNEL
  mutex_type m_named_object_directory_mutex;
#endif
};

} // namespace kernel
} // namespace metall

#endif //METALL_KERNEL_MANAGER_KERNEL_HPP

#include <metall/kernel/manager_kernel_impl.ipp>
#include <metall/kernel/manager_kernel_profile_impl.ipp>
