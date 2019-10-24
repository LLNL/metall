// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_V0_KERNEL_MANAGER_KERNEL_HPP
#define METALL_V0_KERNEL_MANAGER_KERNEL_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <vector>
#include <map>

#include <metall/offset_ptr.hpp>
#include <metall/v0/kernel/manager_kernel_fwd.hpp>
#include <metall/v0/kernel/segment_header.hpp>
#include <metall/v0/kernel/segment_storage/multifile_backed_segment_storage.hpp>
#include <metall/v0/kernel/segment_allocator.hpp>
#include <metall/v0/kernel/named_object_directory.hpp>
#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/in_place_interface.hpp>
#include <metall/detail/utility/array_construct.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/file_clone.hpp>
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

template <typename _chunk_no_type, std::size_t _chunk_size, typename _internal_data_allocator_type>
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
  using internal_data_allocator_type = _internal_data_allocator_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = manager_kernel<_chunk_no_type, _chunk_size, _internal_data_allocator_type>;
  static constexpr const char *k_datastore_dir_name = "metall_datastore";

  // For segment
  static constexpr size_type k_default_vm_reserve_size = 1ULL << 43ULL; // TODO: get from somewhere else
  static constexpr size_type k_max_segment_size = 1ULL << 48ULL; // TODO: get from somewhere else
  using segment_header_type = segment_header<k_chunk_size>;
  static constexpr size_type k_initial_segment_size = 1ULL << 28ULL; // TODO: get from somewhere else
  static constexpr const char *k_segment_prefix = "segment";
  using segment_storage_type = multifile_backed_segment_storage<difference_type, size_type>;

  // Actual memory allocation layer
  static constexpr const char *k_segment_memory_allocator_prefix = "segment_memory_allocator";
  using segment_memory_allocator = segment_allocator<chunk_no_type, size_type, difference_type,
                                                     k_chunk_size, k_max_segment_size,
                                                     segment_storage_type,
                                                     internal_data_allocator_type>;

  // For named object directory
  using named_object_directory_type = named_object_directory<difference_type, size_type, internal_data_allocator_type>;
  static constexpr const char *k_named_object_directory_prefix = "named_object_directory";

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit manager_kernel(const internal_data_allocator_type &allocator);
  ~manager_kernel();

  manager_kernel(const manager_kernel &) = delete;
  manager_kernel &operator=(const manager_kernel &) = delete;

  manager_kernel(manager_kernel &&) = default;
  manager_kernel &operator=(manager_kernel &&) = default;

 public:
  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Expect to be called by a single thread
  /// \param path
  /// \param vm_reserve_size
  void create(const char *path, size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Expect to be called by a single thread
  /// \param path
  /// \return
  bool open(const char *path, bool read_only, size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Expect to be called by a single thread
  void close();

  /// \brief Sync with backing files
  /// \param sync If true, performs synchronous synchronization;
  /// otherwise, performs asynchronous synchronization
  void sync(bool sync);

  /// \brief Allocates memory space
  /// \param nbytes
  /// \return
  void *allocate(size_type nbytes);

  // \TODO: implement
  void *allocate_aligned(size_type nbytes, size_type alignment);

  /// \brief Deallocates
  /// \param addr
  void deallocate(void *addr);

  /// \brief Finds an already constructed object
  /// \tparam T
  /// \param name
  /// \return
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name);

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
  /// \return Returns the adress of the segment header
  segment_header_type *get_segment_header() const;

  /// \brief
  /// \param destination_dir_path
  /// \return
  bool snapshot(const char *destination_dir_path);

  /// \brief Copies backing files synchronously
  /// \param source_dir_path
  /// \param destination_dir_path
  /// \return If succeeded, returns True; other false
  static bool copy(const char *source_dir_path, const char *destination_dir_path);

  /// \brief Copies backing files asynchronously
  /// \param source_dir_path
  /// \param destination_dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> copy_async(const char *source_dir_path, const char *destination_dir_path);

  /// \brief Remove backing files synchronously
  /// \param dir_path
  /// \return If succeeded, returns True; other false
  static bool remove(const char *dir_path);

  /// \brief Remove backing files asynchronously
  /// \param dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> remove_async(const char *dir_path);

  /// \brief Show some profile infromation
  /// \tparam out_stream_type
  /// \param log_out
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) const;

 private:
  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  static std::string priv_make_datastore_dir_path(const std::string &base_dir_path);
  static std::string priv_make_file_name(const std::string &base_dir_path, const std::string &item_name);
  static bool priv_init_datastore_directory(const std::string &base_dir_path);

  bool priv_initialized() const;

  template <typename T>
  T *priv_generic_named_construct(const char_type *name,
                                  size_type num,
                                  bool try2find,
                                  bool, // TODO implement 'dothrow'
                                  util::in_place_interface &table);

  // ---------------------------------------- For segment ---------------------------------------- //
  bool priv_reserve_vm_region(size_type nbytes);
  bool priv_release_vm_region();
  bool priv_allocate_segment_header(void *addr);
  bool priv_deallocate_segment_header();

  // ---------------------------------------- For serializing/deserializing ---------------------------------------- //
  bool priv_serialize_management_data();
  bool priv_deserialize_management_data();

  // ---------------------------------------- File operations ---------------------------------------- //
  /// \brief Copies all backing files using reflink if possible
  static bool priv_copy_data_store(const std::string &src_dir_path, const std::string &dst_dir_path, bool overwrite);

  /// \brief Removes all backing files
  static bool priv_remove_data_store(const std::string &dir_path);

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  std::string m_base_dir_path;
  size_type m_vm_region_size;
  void *m_vm_region;
  size_type m_segment_header_size;
  segment_header_type *m_segment_header;
  named_object_directory_type m_named_object_directory;
  segment_storage_type m_segment_storage;
  segment_memory_allocator m_segment_memory_allocator;

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  mutex_type m_named_object_directory_mutex;
#endif
};

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_V0_KERNEL_MANAGER_KERNEL_HPP

#include <metall/v0/kernel/manager_kernel_impl.ipp>
#include <metall/v0/kernel/manager_kernel_profile_impl.ipp>
