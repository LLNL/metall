// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_MANAGER_KERNEL_HPP
#define METALL_KERNEL_MANAGER_KERNEL_HPP

#include <iostream>
#include <fstream>
#include <streambuf>
#include <cassert>
#include <string>
#include <utility>
#include <memory>
#include <future>
#include <vector>
#include <map>
#include <sstream>
#include <typeinfo>

#include <metall/logger.hpp>
#include <metall/offset_ptr.hpp>
#include <metall/version.hpp>
#include <metall/kernel/manager_kernel_fwd.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/segment_allocator.hpp>
#include <metall/kernel/attributed_object_directory.hpp>
#include <metall/object_attribute_accessor.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/detail/file.hpp>
#include <metall/detail/file_clone.hpp>
#include <metall/detail/char_ptr_holder.hpp>
#include <metall/detail/uuid.hpp>
#include <metall/detail/ptree.hpp>
#include <metall/detail/properly_closed_mark.hpp>

#ifndef METALL_DISABLE_CONCURRENCY
#define METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
#endif
#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
#include <metall/detail/mutex.hpp>
#endif

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

template <typename _storage, typename _segment_storage, typename _chunk_no_type,
          std::size_t _chunk_size>
class manager_kernel {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using void_pointer = offset_ptr<void>;
  using char_type = char;  // required by boost's named proxy
  using char_ptr_holder_type = mdtl::char_ptr_holder<char_type>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using id_type = uint16_t;

  using instance_kind = mdtl::instance_kind;

  using chunk_no_type = _chunk_no_type;
  static constexpr size_type k_chunk_size = _chunk_size;

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //
  using self_type =
      manager_kernel<_storage, _segment_storage, _chunk_no_type, _chunk_size>;
  static constexpr const char *k_management_dir_name = "management";

  // For segment
#ifndef METALL_DEFAULT_CAPACITY
#error "METALL_DEFAULT_CAPACITY is not defined."
#endif
  static constexpr size_type k_default_vm_reserve_size =
      METALL_DEFAULT_CAPACITY;
  static_assert(k_chunk_size <= k_default_vm_reserve_size,
                "Chunk size must be <= k_default_vm_reserve_size");

#ifndef METALL_MAX_CAPACITY
#error "METALL_MAX_CAPACITY is not defined."
#endif
  static constexpr size_type k_max_segment_size = METALL_MAX_CAPACITY;
  static_assert(k_default_vm_reserve_size <= k_max_segment_size,
                "k_default_vm_reserve_size must be <= k_max_segment_size");

  using segment_header_type = segment_header;
  static constexpr size_type k_segment_header_size =
      mdtl::round_up(sizeof(segment_header_type), k_chunk_size);

  using storage = _storage;
  using segment_storage = _segment_storage;

  // For actual memory allocation layer
  static constexpr const char *k_segment_memory_allocator_prefix =
      "segment_memory_allocator";
  using segment_memory_allocator =
      segment_allocator<chunk_no_type, size_type, difference_type, k_chunk_size,
                        k_max_segment_size, segment_storage>;

  // For attributed object directory
  using attributed_object_directory_type =
      attributed_object_directory<difference_type, size_type>;
  static constexpr const char *k_named_object_directory_prefix =
      "named_object_directory";
  static constexpr const char *k_unique_object_directory_prefix =
      "unique_object_directory";
  static constexpr const char *k_anonymous_object_directory_prefix =
      "anonymous_object_directory";

  static constexpr const char *k_properly_closed_mark_file_name =
      "properly_closed_mark";

  // For manager metadata data
  static constexpr const char *k_manager_metadata_file_name =
      "manager_metadata";
  static constexpr const char *k_manager_metadata_key_for_version = "version";
  static constexpr const char *k_manager_metadata_key_for_uuid = "uuid";

  static constexpr const char *k_description_file_name = "description";

  using json_store = mdtl::ptree::node_type;

#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
  using mutex_type = mdtl::mutex;
  using lock_guard_type = mdtl::mutex_lock_guard;
#endif

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using const_named_iterator = attributed_object_directory_type::const_iterator;
  using const_unique_iterator =
      attributed_object_directory_type::const_iterator;
  using const_anonymous_iterator =
      attributed_object_directory_type::const_iterator;
  using named_object_attr_accessor_type =
      named_object_attr_accessor<attributed_object_directory_type::offset_type,
                                 attributed_object_directory_type::size_type>;
  using unique_object_attr_accessor_type =
      unique_object_attr_accessor<attributed_object_directory_type::offset_type,
                                  attributed_object_directory_type::size_type>;
  using anonymous_object_attr_accessor_type = anonymous_object_attr_accessor<
      attributed_object_directory_type::offset_type,
      attributed_object_directory_type::size_type>;
  using path_type = typename storage::path_type;

  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  manager_kernel();
  ~manager_kernel() noexcept;

  manager_kernel(const manager_kernel &) = delete;
  manager_kernel &operator=(const manager_kernel &) = delete;

  manager_kernel(manager_kernel &&) = delete;
  manager_kernel &operator=(manager_kernel &&) = delete;

 public:
  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief Creates a new datastore
  /// Expect to be called by a single thread
  /// \param base_path
  /// \param vm_reserve_size
  /// \return Returns true if success; otherwise, returns false
  bool create(const path_type &base_path,
              size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Opens an existing datastore
  /// Expect to be called by a single thread
  /// \param base_path
  /// \param vm_reserve_size
  /// \return Returns true if success; otherwise, returns false
  bool open(const path_type &base_path,
            size_type vm_reserve_size = k_default_vm_reserve_size);

  /// \brief Opens an existing datastore with read only
  /// Expect to be called by a single thread
  /// \param base_path
  /// \return Returns true if success; otherwise, returns false
  bool open_read_only(const path_type &base_path);

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

  /// \brief Allocate nbytes bytes of uninitialized storage whose alignment is
  /// specified by alignment. \param nbytes A size to allocate. Must be a
  /// multiple of alignment. \param alignment An alignment requirement.
  /// Alignment must be a power of two and satisfy [min allocation size, chunk
  /// size]. \return On success, returns the pointer to the beginning of newly
  /// allocated memory. Returns nullptr, if the given arguments do not satisfy
  /// the requirements above.
  void *allocate_aligned(size_type nbytes, size_type alignment);

  /// \brief Deallocates
  /// \param addr
  void deallocate(void *addr);

  /// \brief Check if all allocated memory has been deallocated.
  /// Note that this function clears object cache.
  bool all_memory_deallocated() const;

  /// \brief Finds an already constructed object
  /// \tparam T
  /// \param name
  /// \return
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) const;

  /// \brief Destroy an already constructed object (named or unique).
  /// Returns true if the object is destroyed.
  /// If name is anonymous, returns false.
  /// \tparam T
  /// \param name
  /// \return
  template <typename T>
  bool destroy(char_ptr_holder_type name);

  /// \brief Destroy a constructed object (named, unique, or anonymous).
  /// Cannot destroy an object not allocated by construct/find_or_construct
  /// functions. \tparam T \param ptr \return
  template <typename T>
  bool destroy_ptr(const T *ptr);

  /// \brief Returns the name of an object created with
  /// construct/find_or_construct functions. If ptr points to an unique
  /// instance, typeid(T).name() is returned. If ptr points to an anonymous
  /// instance or a memory not allocated by construct/find_or_construct
  /// functions, 0 is returned. \tparam T \param ptr \return
  template <class T>
  const char_type *get_instance_name(const T *ptr) const;

  /// \brief Returns is the kind of an object created with
  /// construct/find_or_construct functions. \tparam T \param ptr \return
  template <class T>
  instance_kind get_instance_kind(const T *ptr) const;

  /// \brief Returns the length of an object created with
  /// construct/find_or_construct functions (1 if is a single element, >=1 if
  /// it's an array). \tparam T \param ptr \return
  template <class T>
  size_type get_instance_length(const T *ptr) const;

  /// \brief Checks if the type of an object, which was created with
  /// construct/find_or_construct functions (1 if is a single element, >=1 if
  /// it's an array), is T. \tparam T \param ptr \return
  template <class T>
  bool is_instance_type(const void *ptr) const;

  /// \brief Gets the description of an object created with
  /// construct/find_or_construct. \tparam T The type of the object. \param ptr
  /// A pointer to the object. \param description A pointer to a string buffer.
  /// \return
  template <class T>
  bool get_instance_description(const T *ptr, std::string *description) const;

  /// \brief Sets a description to an object created with
  /// construct/find_or_construct. \tparam T The type of the object. \param ptr
  /// A pointer to the object. \param description A description to set. \return
  template <class T>
  bool set_instance_description(const T *ptr, const std::string &description);

  /// \brief Returns Returns the number of named objects stored in the managed
  /// segment. \return
  size_type get_num_named_objects() const;

  /// \brief Returns Returns the number of unique objects stored in the managed
  /// segment. \return
  size_type get_num_unique_objects() const;

  /// \brief Returns Returns the number of anonymous objects stored in the
  /// managed segment. \return
  size_type get_num_anonymous_objects() const;

  /// \brief Returns a constant iterator to the index storing the named objects.
  /// \return
  const_named_iterator named_begin() const;

  /// \brief Returns a constant iterator to the end of the index storing the
  /// named allocations. \return
  const_named_iterator named_end() const;

  /// \brief Returns a constant iterator to the index storing the unique
  /// objects. \return
  const_unique_iterator unique_begin() const;

  /// \brief Returns a constant iterator to the end of the index
  /// storing the unique allocations. NOT thread-safe. Never throws.
  /// \return
  const_unique_iterator unique_end() const;

  /// \brief Returns a constant iterator to the index storing the anonymous
  /// objects. \return
  const_anonymous_iterator anonymous_begin() const;

  /// \brief Returns a constant iterator to the end of the index
  /// storing the anonymous allocations. NOT thread-safe. Never throws.
  /// \return
  const_anonymous_iterator anonymous_end() const;

  /// \brief Generic named/anonymous new function. This method is required by
  /// construct_proxy and construct_iter_proxy. \tparam T Type of the object(s).
  /// \param name Name of the object(s).
  /// \param num Number of objects to be constructed.
  /// \param try2find If true, tries to find already constructed object(s) with
  /// the same name. \param do_throw Ignored. This method does not throw its own
  /// exception --- this function throws an exception thrown by the constructor
  /// of the object. This is how Boost.Interprocess treats this parameter.
  /// \param pr Reference to a CtorArgN object.
  /// \return Returns a pointer to the constructed object(s).
  template <typename T, typename proxy>
  T *generic_construct(char_ptr_holder_type name, size_type num, bool try2find,
                       bool do_throw, proxy &pr);

  /// \brief Return a reference to the segment header.
  /// \return A reference to the segment header.
  const segment_header_type &get_segment_header() const;

  /// \brief Get the address of the application segment segment.
  /// \return Returns the address of the application segment segment.
  const void *get_segment() const;

  /// \brief Get the size of the application data segment.
  /// \return Returns the size of the application data segment.
  size_type get_segment_size() const;

  /// \brief Returns if this kernel was opened as read-only
  /// \return whether this kernel is read-only
  bool read_only() const;

  /// \brief Takes a snapshot. The snapshot has a different UUID.
  /// \param destination_base_path Destination path
  /// \param clone Use clone (reflink) to copy data.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return If succeeded, returns True; other false
  bool snapshot(const path_type &destination_base_path, bool clone,
                int num_max_copy_threads);

  /// \brief Copies a data store synchronously, keeping the same UUID.
  /// \param source_base_path Source path.
  /// \param destination_base_path Destination path.
  /// \param clone Use clone (reflink) to copy data.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return If succeeded, returns True; other false.
  static bool copy(const path_type &source_base_path,
                   const path_type &destination_base_path, bool clone,
                   int num_max_copy_threads);

  /// \brief Copies a data store asynchronously, keeping the same UUID.
  /// \param source_base_path Source path.
  /// \param destination_base_path Destination path.
  /// \param clone Use clone (reflink) to copy data.
  /// \param num_max_copy_threads The maximum number of copy threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Returns an object of std::future.
  /// If succeeded, its get() returns True; other false.
  static std::future<bool> copy_async(const path_type &source_base_path,
                                      const path_type &destination_base_path,
                                      bool clone, int num_max_copy_threads);

  /// \brief Remove a data store synchronously
  /// \param base_path
  /// \return If succeeded, returns True; other false
  static bool remove(const path_type &base_path);

  /// \brief Remove a data store asynchronously
  /// \param base_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> remove_async(const path_type &base_path);

  /// \brief Check if the backing data store is consistent,
  /// i.e. it was closed properly.
  /// \param dir_path
  /// \return Return true if it is consistent; otherwise, returns false.
  static bool consistent(const path_type &dir_path);

  /// \brief Returns the UUID of the backing data store.
  /// \return Returns UUID in std::string; returns an empty string on error.
  std::string get_uuid() const;

  /// \brief Returns the UUID of the backing data store.
  /// \param base_path Path to a data store.
  /// \return Returns UUID in std::string; returns an empty string on error.
  static std::string get_uuid(const path_type &base_path);

  /// \brief Gets the version number of the backing data store.
  /// \return Returns a version number; returns 0 on error.
  version_type get_version() const;

  /// \brief Gets the version number of the backing data store.
  /// \param base_path Path to a data store.
  /// \return Returns a version number; returns 0 on error.
  static version_type get_version(const path_type &base_path);

  /// \brief Gets a description from a file.
  /// \param description A pointer to a string buffer.
  /// \return Returns false on error.
  bool get_description(std::string *description) const;

  /// \brief Gets a description from a file.
  /// \param base_path  Path to a data store.
  /// \param description A pointer to a string buffer.
  /// \return Returns false on error.
  static bool get_description(const path_type &base_path,
                              std::string *description);

  /// \brief Sets a description to a file.
  /// \param description A description to write.
  /// \return Returns false on error.
  bool set_description(const std::string &description);

  /// \brief Sets a description to a file.
  /// \param base_path Path to a data store.
  /// \param description A description to write.
  /// \return Returns false on error.
  static bool set_description(const path_type &base_path,
                              const std::string &description);

  /// \brief Returns an instance that provides access to the attribute of named
  /// objects. \param base_path Path to a data store. \return Returns an
  /// instance of named_object_attr_accessor_type.
  static named_object_attr_accessor_type access_named_object_attribute(
      const path_type &base_path);

  /// \brief Returns an instance that provides access to the attribute of unique
  /// object. \param base_path Path to a data store. \return Returns an
  /// instance of unique_object_attr_accessor_type.
  static unique_object_attr_accessor_type access_unique_object_attribute(
      const path_type &base_path);

  /// \brief Returns an instance that provides access to the attribute of
  /// anonymous object. \param base_path Path to a data store. \return
  /// Returns an instance of anonymous_object_attr_accessor_type.
  static anonymous_object_attr_accessor_type access_anonymous_object_attribute(
      const path_type &base_path);

  /// \brief Checks if the status of this kernel is good.
  /// \return Returns true if it is good; otherwise, returns false.
  bool good() const noexcept;

  /// \brief Show some profile information.
  /// This method release object caches (which will slow down Metall).
  /// \tparam out_stream_type
  /// \param log_out
  template <typename out_stream_type>
  void profile(out_stream_type *log_out);

 private:
  // -------------------- //
  // Private methods
  // -------------------- //

  void priv_check_sanity() const;
  bool priv_validate_runtime_configuration() const;
  difference_type priv_to_offset(const void *ptr) const;
  void *priv_to_address(difference_type offset) const;

  // ---------- For data store structure  ---------- //
  // Directory structure:
  // base_path/ <- this path is given by user
  //  top_dir/ <- managed by storage class
  //    some top-level files
  //    management_dir/
  //      directories and files for allocation management
  //    segment_dir/ <- managed by segment_storage class
  //      directories and files for application data segment

  static bool priv_create_datastore_directory(const path_type &base_path);

  // ---------- For consistence support  ---------- //
  static bool priv_consistent(const path_type &base_path);
  static bool priv_check_version(const json_store &metadata_json);
  static bool priv_properly_closed(const path_type &base_path);
  static bool priv_mark_properly_closed(const path_type &base_path);

  // ---------- For constructed objects  ---------- //
  template <typename T, typename proxy>
  T *priv_generic_construct(char_ptr_holder_type name, size_type length,
                            bool try2find, proxy &pr);

  template <typename T>
  bool priv_register_attr_object_no_mutex(char_ptr_holder_type name,
                                          difference_type offset,
                                          size_type length);

  bool priv_remove_attr_object_no_mutex(difference_type offset);

  template <typename T>
  void priv_destruct_and_free_memory(difference_type offset, size_type length);

  // ---------- For segment  ---------- //
  bool priv_open(const path_type &base_path, bool read_only,
                 size_type vm_reserve_size_request = 0);
  bool priv_create(const path_type &base_path, size_type vm_reserve_size);

  // ---------- For serializing/deserializing  ---------- //
  bool priv_serialize_management_data();
  bool priv_deserialize_management_data();

  // ---------- snapshot  ---------- //
  /// \brief Takes a snapshot. The snapshot has a different UUID.
  bool priv_snapshot(const path_type &destination_base_path, bool clone,
                     int num_max_copy_threads);

  // ---------- File operations  ---------- //
  /// \brief Copies all backing files using reflink if possible
  static bool priv_copy_data_store(const path_type &src_base_path,
                                   const path_type &dst_base_path, bool clone,
                                   int num_max_copy_threads);

  /// \brief Removes all backing files
  static bool priv_remove_data_store(const path_type &base_path);

  // ---------- Management metadata  ---------- //
  static bool priv_read_management_metadata(const path_type &base_path,
                                            json_store *json_root);
  static bool priv_write_management_metadata(const path_type &base_path,
                                             const json_store &json_root);

  static version_type priv_get_version(const json_store &metadata_json);
  static bool priv_set_version(json_store *metadata_json);

  static bool priv_set_uuid(json_store *metadata_json);
  static std::string priv_get_uuid(const json_store &metadata_json);

  // ---------- Description  ---------- //
  static bool priv_read_description(const path_type &base_path,
                                    std::string *description);
  static bool priv_write_description(const path_type &base_path,
                                     const std::string &description);

  // -------------------- //
  // Private fields
  // -------------------- //
  bool m_good{false};
  path_type m_base_path{};
  mtlldetail::properly_closed_mark m_properly_closed_mark{};
  attributed_object_directory_type m_named_object_directory{};
  attributed_object_directory_type m_unique_object_directory{};
  attributed_object_directory_type m_anonymous_object_directory{};
  segment_memory_allocator m_segment_memory_allocator{nullptr};
  std::unique_ptr<json_store> m_manager_metadata{nullptr};
  segment_storage m_segment_storage{};

#ifdef METALL_ENABLE_MUTEX_IN_MANAGER_KERNEL
  std::unique_ptr<mutex_type> m_object_directories_mutex{nullptr};
#endif
};

}  // namespace kernel
}  // namespace metall

#endif  // METALL_KERNEL_MANAGER_KERNEL_HPP

#include <metall/kernel/manager_kernel_impl.ipp>
#include <metall/kernel/manager_kernel_profile_impl.ipp>
