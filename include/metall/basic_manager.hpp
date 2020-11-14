// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_BASIC_MANAGER_HPP
#define METALL_BASIC_MANAGER_HPP

#include <cstddef>
#include <memory>

#include <metall/tags.hpp>
#include <metall/stl_allocator.hpp>
#include <metall/kernel/manager_kernel.hpp>
#include <metall/detail/utility/named_proxy.hpp>

namespace metall {

#if !defined(DOXYGEN_SKIP)
// Forward declaration
template <typename chunk_no_type, std::size_t k_chunk_size, typename kernel_allocator_type>
class basic_manager;
#endif // DOXYGEN_SKIP

/// \brief A generalized Metall manager class
/// \tparam chunk_no_type
/// \tparam k_chunk_size
/// \tparam kernel_allocator_type
template <typename chunk_no_type = uint32_t, std::size_t k_chunk_size = (1ULL << 21ULL), typename kernel_allocator_type = std::allocator<std::byte>>
class basic_manager {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //

  /// \brief Manager kernel type
  using manager_kernel_type = kernel::manager_kernel<chunk_no_type, k_chunk_size, kernel_allocator_type>;

  /// \brief Void pointer type
  using void_pointer = typename manager_kernel_type::void_pointer;

  /// \brief Size type
  using size_type = typename manager_kernel_type::size_type;

  /// \brief Difference type
  using difference_type = typename manager_kernel_type::difference_type;

  /// \brief Allocator type
  template <typename T>
  using allocator_type = stl_allocator<T, manager_kernel_type>;

  /// \brief Construct proxy
  template <typename T>
  using construct_proxy =  metall::detail::utility::named_proxy<manager_kernel_type, T, false>;

  /// \brief Construct iterator proxy
  template <typename T>
  using construct_iter_proxy =  metall::detail::utility::named_proxy<manager_kernel_type, T, true>;

  /// \brief Chunk number type (= chunk_no_type)
  using chunk_number_type = chunk_no_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using char_ptr_holder_type = typename manager_kernel_type::char_ptr_holder_type;
  using self_type = basic_manager<chunk_no_type, k_chunk_size, kernel_allocator_type>;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //

  /// \brief Opens an existing data store.
  /// \param base_path Path to a data store.
  /// \param allocator Allocator to allocate management data.
  basic_manager(open_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.open(base_path);
  }

  /// \brief Opens an existing data store with the read only mode.
  /// Write accesses will cause segmentation fault.
  /// \param base_path Path to a data store.
  /// \param allocator Allocator to allocate management data.
  basic_manager(open_read_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.open_read_only(base_path);
  }

  /// \brief Creates a new data store (an existing data store will be overwritten).
  /// \param base_path Path to create a data store.
  /// \param allocator Allocator to allocate management data.
  basic_manager(create_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.create(base_path);
  }

  /// \brief Creates a new data store (an existing data store will be overwritten).
  /// \param base_path Path to create a data store.
  /// \param capacity Maximum total allocation size.
  /// \param allocator Allocator to allocate management data.
  basic_manager(create_only_t, const char *base_path, const size_type capacity,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.create(base_path, capacity);
  }

  /// \brief Deleted.
  basic_manager() = delete;

  /// \brief Destructor.
  ~basic_manager() noexcept = default;

  /// \brief Deleted.
  basic_manager(const basic_manager &) = delete;

  /// \brief Move constructor.
  basic_manager(basic_manager &&) noexcept = default;

  /// \brief Deleted.
  /// \return N/A.
  basic_manager &operator=(const basic_manager &) = delete;

  /// \brief Move assignment operator.
  /// \return An reference to the object.
  basic_manager &operator=(basic_manager &&) noexcept = default;

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //

  // -------------------- Object construction function family -------------------- //
  // Each function also works with '[ ]' operator to generate an array, leveraging the proxy class (construct_proxy)

  /// \private
  /// \class common_doc_const_find
  /// \brief
  /// Object construction API developed by Boost.Interprocess
  /// <a href="https://www.boost.org/doc/libs/release/doc/html/interprocess/managed_memory_segments.html#interprocess.managed_memory_segments.managed_memory_segment_features.allocation_types">
  /// (see details).
  /// </a>

  /// \brief Allocates an object of type T (throwing version).
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// T *ptr = basic_manager.construct<T>("Name")(arg1, arg2...);
  /// T *ptr = basic_manager.construct<T>("Name")[count](arg1, arg2...);
  /// \endcode
  /// Where, 'arg1, arg2...' are the arguments passed to T's constructor via a proxy object.
  /// One can also construct an array using '[ ]' operator. When an array is constructed, each object receives the same arguments.
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that constructs the object on the allocated space.
  template <typename T>
  construct_proxy<T> construct(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, false, true);
  }

  /// \brief Tries to find an already constructed object. If not exist, constructs an object of type T (throwing version).
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// T *ptr = basic_manager.find_or_construct<T>("Name")(arg1, arg2...);
  /// T *ptr = basic_manager.find_or_construct<T>("Name")[count](arg1, arg2...);
  /// \endcode
  /// Where, 'arg1, arg2...' are the arguments passed to T's constructor via a proxy object.
  /// One can also construct an array using '[ ]' operator. When an array is constructed, each object receives the same arguments.
  ///
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return A proxy object that holds a pointer of an already constructed object or an object newly constructed.
  template <typename T>
  construct_proxy<T> find_or_construct(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, true, true);
  }

  /// \brief Allocates an array of objects of type T, receiving arguments from iterators (throwing version).
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// T *ptr = basic_manager.construct_it<T>("Name")[count](it1, it2...);
  /// \endcode
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object to construct an array of objects.
  template <typename T>
  construct_iter_proxy<T> construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, false, true);
  }

  /// \brief Tries to find an already constructed object.
  /// If not exist, constructs an array of objects of type T, receiving arguments from iterators (throwing version).
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// T *ptr = basic_manager.find_or_construct_it<T>("Name")[count](it1, it2...);
  /// \endcode
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  ///
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that holds a pointer to the already constructed object or
  /// constructs an array of objects or just holds an pointer.
  template <typename T>
  construct_iter_proxy<T> find_or_construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, true, true);
  }

  /// \brief Tries to find a previously created object.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// std::pair<T *, std::size_t> ret = basic_manager.find<T>("Name");
  /// \endcode
  ///
  /// \tparam T  The type of the object.
  /// \param name The name of the object.
  /// \return Returns a pointer to the object and the count (if it is not an array, returns 1).
  /// If not present, nullptr is returned.
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) const {
    return m_kernel.template find<T>(name);
  }

  /// \brief Destroys a previously created object.
  /// \copydoc common_doc_const_find
  ///
  /// \details
  /// Example:
  /// \code
  /// bool destroyed = basic_manager.destroy<T>("Name");
  /// \endcode
  ///
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return Returns false if the object was not present.
  template <typename T>
  bool destroy(char_ptr_holder_type name) {
    return m_kernel.template destroy<T>(name);
  }

  // -------------------- Allocate memory by size -------------------- //
  /// \brief Allocates nbytes bytes.
  /// \param nbytes Number of bytes to allocate.
  /// \return Returns a pointer to the allocated memory.
  void *allocate(size_type nbytes) {
    return m_kernel.allocate(nbytes);
  }

  /// \brief Allocates nbytes bytes. The address of the allocated memory will be a multiple of alignment.
  /// \param nbytes Number of bytes to allocate. Must be a multiple alignment.
  /// \param alignment Alignment size.
  /// Alignment must be a power of two and satisfy [min allocation size, chunk size].
  /// \return Returns a pointer to the allocated memory.
  void *allocate_aligned(size_type nbytes,
                         size_type alignment) {
    return m_kernel.allocate_aligned(nbytes, alignment);
  }

  // void allocate_many(const std::nothrow_t &tag, size_type elem_bytes, size_type n_elements, multiallocation_chain &chain);

  /// \brief Deallocates the allocated memory.
  /// \param addr A pointer to the allocated memory to be deallocated.
  void deallocate(void *addr) {
    return m_kernel.deallocate(addr);
  }

  // void deallocate_many(multiallocation_chain &chain);

  // -------------------- Flush -------------------- //
  /// \brief Flush data to persistent memory.
  /// \param synchronous If true, performs synchronous operation;
  /// otherwise, performs asynchronous operation.
  void flush(const bool synchronous = true) {
    m_kernel.flush(synchronous);
  }

  // -------------------- Utility Methods -------------------- //
  /// \brief Returns a STL compatible allocator object.
  /// \tparam T Type of the object.
  /// \return Returns a STL compatible allocator object.
  template <typename T = std::byte>
  allocator_type<T> get_allocator() const {
    return allocator_type<T>(reinterpret_cast<manager_kernel_type **>(&(m_kernel.get_segment_header()->manager_kernel_address)));
  }

  /// \brief Takes a snapshot of the current data. The snapshot has a new UUID.
  /// \param destination_dir_path Path to store a snapshot.
  /// \return Returns true on success; other false.
  bool snapshot(const char *destination_dir_path) {
    return m_kernel.snapshot(destination_dir_path);
  }

  /// \brief Copies data store synchronously.
  /// \param source_dir_path Source data store path.
  /// \param destination_dir_path Destination data store path.
  /// \return If succeeded, returns true; other false.
  static bool copy(const char *source_dir_path, const char *destination_dir_path) {
    return manager_kernel_type::copy(source_dir_path, destination_dir_path);
  }

  /// \brief Copies data store asynchronously.
  /// \param source_dir_path Source data store path.
  /// \param destination_dir_path Destination data store path.
  /// \return Returns an object of std::future.
  /// If succeeded, its get() returns true; other false.
  static auto copy_async(const char *source_dir_path, const char *destination_dir_path) {
    return manager_kernel_type::copy_async(source_dir_path, destination_dir_path);
  }

  /// \brief Remove data store synchronously.
  /// \param dir_path Path to a data store to remove.
  /// \return If succeeded, returns true; other false.
  static bool remove(const char *dir_path) {
    return manager_kernel_type::remove(dir_path);
  }

  /// \brief Remove data store asynchronously.
  /// \param dir_path Path to a data store to remove.
  /// \return Returns an object of std::future.
  /// If succeeded, its get() returns true; other false
  static std::future<bool> remove_async(const char *dir_path) {
    return std::async(std::launch::async, remove, dir_path);
  }

  /// \brief Check if the backing data store exists and is consistent (i.e., it was closed properly in the previous run).
  /// \param dir_path Path to a data store.
  /// \return Returns true if it exists and is consistent; otherwise, returns false.
  static bool consistent(const char *dir_path) {
    return manager_kernel_type::consistent(dir_path);
  }

  /// \brief Returns the internal chunk size.
  /// \return The size of internal chunk size.
  static constexpr size_type chunk_size() {
    return k_chunk_size;
  }

  /// \brief Returns a pointer to an object of manager_kernel_type class.
  /// \return Returns a pointer to an object of manager_kernel_type class.
  manager_kernel_type *get_kernel() {
    return &m_kernel;
  }

  /// \brief Returns a UUID of the data store.
  /// \param dir_path Path to a data store.
  /// \return UUID in the std::string format; returns an empty string on error.
  static std::string get_uuid(const char *dir_path) {
    return manager_kernel_type::get_uuid(dir_path);
  }

  // -------------------- For profiling and debug -------------------- //
#if !defined(DOXYGEN_SKIP)
  /// \brief Prints out profiling information.
  /// \tparam out_stream_type A type of out stream.
  /// \param log_out An object of the out stream.
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) const {
    m_kernel.profile(log_out);
  }
#endif

 private:
  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  manager_kernel_type m_kernel;
};
} // namespace metall

#endif //METALL_BASIC_MANAGER_HPP
