// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_BASE_MANAGER_HPP
#define METALL_DETAIL_BASE_MANAGER_HPP

#include <utility>
#include <metall/tags.hpp>
#include <metall/detail/utility/char_ptr_holder.hpp>

namespace metall {
namespace detail {

// Forward declaration
template <typename impl_type, typename type_holder>
class base_manager;

/// \brief An interface class of manager classes.
///
/// This class is designed as a base class of manager with Curiously Recurring Template Pattern (CRTP)
/// to provide Boost.Interprocess like APIs. impl_type could have additional its original APIs.
/// The actual memory allocation algorithm is handled by manage_kernel class.
/// \tparam impl_type Delivered class which has actual implementation
/// \tparam type_holder A utility struct that holds internal types of the delivered class
template <typename impl_type, typename type_holder>
class base_manager {
 public:
  using void_pointer = typename type_holder::void_pointer;
  using size_type = typename type_holder::size_type;
  template <typename T>
  using allocator_type = typename type_holder::template allocator_type<T>;
  template <typename T>
  using construct_proxy = typename type_holder::template construct_proxy<T>;
  template <typename T>
  using construct_iter_proxy = typename type_holder::template construct_iter_proxy<T>;
  using manager_kernel_type = typename type_holder::kernel_type;

 private:
  using char_ptr_holder_type = typename manager_kernel_type::char_ptr_holder_type;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  base_manager() = default;
  ~base_manager() = default;

  base_manager(const base_manager &) = delete;
  base_manager &operator=(const base_manager &) = delete;

  base_manager(base_manager &&) = default;
  base_manager &operator=(base_manager &&) = default;

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
    return impl().template construct_impl<T>(name);
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
    return impl().template find_or_construct_impl<T>(name);
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
    return impl().template construct_it_impl<T>(name);
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
    return impl().template find_or_construct_it_impl<T>(name);
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
    return impl().template find_impl<T>(name);
  }

  /// \brief Destroys a previously created unique instance.
  /// \tparam T The type of the object.
  /// \param name The name of the object.
  /// \return Returns false if the object was not present.
  template <typename T>
  bool destroy(char_ptr_holder_type name) {
    return impl().template destroy_impl<T>(name);
  }

  // -------------------- Allocate memory by size -------------------- //
  /// \brief Allocates nbytes bytes.
  /// \param nbytes Number of bytes to allocate
  /// \return Returns a pointer to the allocated memory
  void *allocate(size_type nbytes) {
    return impl().allocate_impl(nbytes);
  }

  /// \brief Allocates nbytes bytes. The address of the allocated memory will be a multiple of alignment.
  /// \param nbytes Number of bytes to allocate
  /// \param alignment Alignment size
  /// \return Returns a pointer to the allocated memory
  void *allocate_aligned(size_type nbytes,
                         size_type alignment) {
    return impl().allocate_aligned_impl(nbytes, alignment);
  }
  // void allocate_many(const std::nothrow_t &tag, size_type elem_bytes, size_type n_elements, multiallocation_chain &chain);

  /// \brief Deallocates the allocated memory
  /// \param addr A pointer to the allocated memory to be deallocated
  void deallocate(void *addr) {
    impl().deallocate_impl(addr);
  }
  // void deallocate_many(multiallocation_chain &chain);

  // -------------------- Sync -------------------- //
  /// \brief Sync with persistent memory
  /// \param sync If true, performs synchronous synchronization;
  /// otherwise, performs asynchronous synchronization
  void sync(const bool sync = true) {
    impl().sync_impl(sync);
  }

  // -------------------- Utility Methods -------------------- //
  /// \brief Returns a pointer to an object of impl_type class
  /// \return Returns a pointer to an object of impl_type class
  manager_kernel_type *get_kernel() {
    return impl().get_kernel_impl();
  }

  /// \brief Returns a STL compatible allocator object
  /// \tparam T Type of the object
  /// \return Returns a STL compatible allocator object
  template <typename T = std::byte>
  allocator_type<T> get_allocator() {
    return impl().get_allocator_impl();
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  impl_type &impl() { return *static_cast<impl_type *>(this); }
  const impl_type &impl() const { return *static_cast<const impl_type *>(this); }
};

} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_BASE_MANAGER_HPP
