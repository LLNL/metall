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
#include <metall/detail/utility/in_place_interface.hpp>
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
  using manager_kernel_type = kernel::manager_kernel<chunk_no_type, k_chunk_size, kernel_allocator_type>;
  using void_pointer = typename manager_kernel_type::void_pointer;
  using size_type = typename manager_kernel_type::size_type;
  using difference_type = typename manager_kernel_type::difference_type;
  template <typename T>
  using allocator_type = stl_allocator<T, manager_kernel_type>;
  template <typename T>
  using construct_proxy =  metall::detail::utility::named_proxy<manager_kernel_type, T, false>;
  template <typename T>
  using construct_iter_proxy =  metall::detail::utility::named_proxy<manager_kernel_type, T, true>;

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
  basic_manager(open_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    const bool read_only = false;
    if (!m_kernel.open(base_path, read_only)) {
      std::cerr << "Cannot open " << base_path << std::endl;
      std::abort();
    }
  }

  basic_manager(open_read_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    const bool read_only = true;
    if (!m_kernel.open(base_path, read_only)) {
      std::cerr << "Cannot open " << base_path << std::endl;
      std::abort();
    }
  }

  basic_manager(create_only_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.create(base_path);
  }

  basic_manager(create_only_t, const char *base_path, const size_type capacity,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    m_kernel.create(base_path, capacity);
  }

  basic_manager(open_or_create_t, const char *base_path, const size_type capacity,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    const bool read_only = false;
    if (!m_kernel.open(base_path, read_only)) {
      m_kernel.create(base_path, capacity);
    }
  }

  basic_manager(open_or_create_t, const char *base_path,
                const kernel_allocator_type &allocator = kernel_allocator_type())
      : m_kernel(allocator) {
    const bool read_only = false;
    if (!m_kernel.open(base_path, read_only)) {
      m_kernel.create(base_path);
    }
  }

  basic_manager() = delete;
  ~basic_manager() = default;
  basic_manager(const basic_manager &) = delete;
  basic_manager(basic_manager &&) = default;
  basic_manager &operator=(const basic_manager &) = delete;
  basic_manager &operator=(basic_manager &&) = default;

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  // -------------------- Object construction function family -------------------- //
  // Each function also works with '[ ]' operator to generate an array leveraging the proxy class (construct_proxy)

  /// \brief Allocates an object of type T (throwing version).
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that constructs the object on the allocated space.
  // Example:
  // MyType *ptr = managed_memory_segment.construct<MyType>("Name") (par1, par2...);
  template <typename T>
  construct_proxy<T> construct(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, false, true);
  }

  /// \brief Finds or constructs an object of type T.
  ///
  /// Tries to find a previously created object.
  /// If not exist, allocates and constructs an object of type T (throwing version).
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return A proxy object that holds a pointer the already constructed object or
  /// constructs the object on the allocated space/
  // Example:
  // MyType *ptr = managed_memory_segment.find_or_construct<MyType>("Name") (par1, par2...);
  template <typename T>
  construct_proxy<T> find_or_construct(char_ptr_holder_type name) {
    return construct_proxy<T>(&m_kernel, name, true, true);
  }

  /// \brief Allocates an array of objects of type T (throwing version).
  ///
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that constructs an array of objects.
  // Example:
  // MyType *ptr = manager.construct_it<MyType>("Name")[count](it1, it2...);
  template <typename T>
  construct_iter_proxy<T> construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, false, true);
  }

  /// \brief Allocates and constructs an array of objects of type T (throwing version)
  ///
  /// Each object receives parameters returned with the expression (*it1++, *it2++,... ).
  /// \tparam T The type of the object.
  /// \param name A unique name of the object.
  /// \return A proxy object that holds a pointer to the already constructed object or
  /// constructs an array of objects or just holds an pointer.
  // Example:
  // MyType *ptr = manager.find_or_construct_it<MyType>("Name")[count](it1, it2...);
  template <typename T>
  construct_iter_proxy<T> find_or_construct_it(char_ptr_holder_type name) {
    return construct_iter_proxy<T>(&m_kernel, name, true, true);
  }

  /// \brief Tries to find a previously created object.
  /// \tparam T  The type of the object.
  /// \param name The name of the object.
  /// \return Returns a pointer to the object and the count (if it is not an array, returns 1).
  /// If not present, the returned pointer is nullptr.
  // Example:
  // std::pair<MyType *,std::size_t> ret = managed_memory_segment.find<MyType>("Name");
  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) {
    return m_kernel.template find<T>(name);
  }

  /// \brief Destroys a previously created unique instance.
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return Returns false if the object was not present.
  template <typename T>
  bool destroy(char_ptr_holder_type name) {
    return m_kernel.template destroy<T>(name);
  }

  // -------------------- Allocate memory by size -------------------- //
  /// \brief Allocates nbytes bytes.
  /// \param nbytes Number of bytes to allocate
  /// \return Returns a pointer to the allocated memory
  void *allocate(size_type nbytes) {
    return m_kernel.allocate(nbytes);
  }

  /// \brief Allocates nbytes bytes. The address of the allocated memory will be a multiple of alignment.
  /// \param nbytes Number of bytes to allocate
  /// \param alignment Alignment size
  /// \return Returns a pointer to the allocated memory
  void *allocate_aligned(size_type nbytes,
                         size_type alignment) {
    return m_kernel.allocate_aligned(nbytes, alignment);
  }

  // void allocate_many(const std::nothrow_t &tag, size_type elem_bytes, size_type n_elements, multiallocation_chain &chain);

  /// \brief Deallocates the allocated memory
  /// \param addr A pointer to the allocated memory to be deallocated
  void deallocate(void *addr) {
    return m_kernel.deallocate(addr);
  }

  // void deallocate_many(multiallocation_chain &chain);

  // -------------------- Flush -------------------- //
  /// \brief Flush data to persistent memory
  /// \param synchronous If true, performs synchronous operation;
  /// otherwise, performs asynchronous operation.
  void flush(const bool synchronous = true) {
    m_kernel.flush(synchronous);
  }

  // -------------------- Utility Methods -------------------- //
  /// \brief Returns a pointer to an object of manager_kernel_type class
  /// \return Returns a pointer to an object of manager_kernel_type class
  manager_kernel_type *get_kernel() {
    return &m_kernel;
  }

  /// \brief Returns a STL compatible allocator object
  /// \tparam T Type of the object
  /// \return Returns a STL compatible allocator object
  template <typename T = std::byte>
  allocator_type<T> get_allocator() {
    return allocator_type<T>(reinterpret_cast<manager_kernel_type **>(&(m_kernel.get_segment_header()->manager_kernel_address)));
  }

  /// \brief Snapshot the entire data
  /// \param destination_dir_path The prefix of the snapshot files
  /// \return Returns true on success; other false
  bool snapshot(const char *destination_dir_path) {
    return m_kernel.snapshot(destination_dir_path);
  }

  /// \brief Copies backing files synchronously
  /// \param source_dir_path
  /// \param destination_dir_path
  /// \return If succeeded, returns True; other false
  static bool copy(const char *source_dir_path, const char *destination_dir_path) {
    return manager_kernel_type::copy(source_dir_path, destination_dir_path);
  }

  /// \brief Copies backing files asynchronously
  /// \param source_dir_path
  /// \param destination_dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static auto copy_async(const char *source_dir_path, const char *destination_dir_path) {
    return manager_kernel_type::copy_async(source_dir_path, destination_dir_path);
  }

  /// \brief Remove backing files synchronously
  /// \param dir_path
  /// \return If succeeded, returns True; other false
  static bool remove(const char *dir_path) {
    return manager_kernel_type::remove(dir_path);
  }

  /// \brief Remove backing files asynchronously
  /// \param dir_path
  /// \return Returns an object of std::future
  /// If succeeded, its get() returns True; other false
  static std::future<bool> remove_async(const char *dir_path) {
    return std::async(std::launch::async, remove, dir_path);
  }

  /// \brief Check if the backing data store is consistent,
  /// i.e. it was closed properly.
  /// \param dir_path
  /// \return Return true if it is consistent; otherwise, returns false.
  static bool consistent(const char *dir_path) {
    return manager_kernel_type::consistent(dir_path);
  }

  /// \brief Returns the chunk size
  /// \return
  static constexpr size_type chunk_size() {
    return k_chunk_size;
  }

  /// \brief Output profile information
  /// \param log_out
  template <typename out_stream_type>
  void profile(out_stream_type *log_out) const {
    m_kernel.profile(log_out);
  }

 private:
  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  manager_kernel_type m_kernel;
};
} // namespace metall

#endif //METALL_BASIC_MANAGER_HPP
