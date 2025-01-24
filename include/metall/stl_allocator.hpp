// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_STL_ALLOCATOR_HPP
#define METALL_STL_ALLOCATOR_HPP

#include <memory>
#include <type_traits>
#include <cassert>
#include <limits>
#include <new>

#include <metall/offset_ptr.hpp>
#include <metall/logger.hpp>

namespace metall {

/// \brief A STL compatible allocator.
/// \tparam T A object type.
/// \tparam metall_manager_kernel_type A manager kernel type.
/// \warning
/// This allocator does not define propagate_on_* types, as same as
/// Boost.Interprocess. Those types are going to be std::false_type in
/// std::allocator_traits. Therefore, this allocator is not propagated during
/// containers' copy assignment, move assignment, or swap. This configuration
/// enables users to copy containers between different Metall managers easier.
/// On the other hand, performing the move assignment between two containers
/// allocated by different Metall managers invokes copy operations instead of
/// move operations. Also, swapping containers allocated by different Metall
/// managers will result in undefined behavior.
template <typename T, typename metall_manager_kernel_type>
class stl_allocator {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using value_type = T;
  using pointer = typename std::pointer_traits<
      typename metall_manager_kernel_type::void_pointer>::
      template rebind<value_type>;
  using const_pointer =
      typename std::pointer_traits<pointer>::template rebind<const value_type>;
  using void_pointer =
      typename std::pointer_traits<pointer>::template rebind<void>;
  using const_void_pointer =
      typename std::pointer_traits<pointer>::template rebind<const void>;
  using difference_type =
      typename std::pointer_traits<pointer>::difference_type;
  using size_type = typename std::make_unsigned<difference_type>::type;
  using manager_kernel_type = metall_manager_kernel_type;

  /// \brief Makes another allocator type for type T2
  /// \tparam T2 The type of the object
  template <typename T2>
  struct rebind {
    using other = stl_allocator<T2, manager_kernel_type>;
  };

 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  // Following manager.hpp in Boost.interprocess, 'explicit' keyword is not used
  // on purpose although this allocator won't work correctly w/o a valid
  // manager_kernel_address. The following code will work:
  //
  // void func(stl_allocator<int, manager_kernel_type>) {...}
  // int main() {
  //   manager_kernel_type** ptr = ...
  //   func(ptr); // OK
  // }
  //
  stl_allocator(
      manager_kernel_type *const *const pointer_manager_kernel_address) noexcept
      : m_ptr_manager_kernel_address(pointer_manager_kernel_address) {}

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  stl_allocator(
      stl_allocator<T2, manager_kernel_type> allocator_instance) noexcept
      : m_ptr_manager_kernel_address(
            allocator_instance.get_pointer_to_manager_kernel()) {}

  /// \brief Copy constructor
  stl_allocator(const stl_allocator &other) noexcept = default;

  /// \brief Move constructor
  stl_allocator(stl_allocator &&other) noexcept = default;

  /// \brief Destructor
  ~stl_allocator() noexcept = default;

  /// \brief Copy assign operator
  stl_allocator &operator=(const stl_allocator &) noexcept = default;

  /// \brief Copy assign operator for another T
  template <typename T2>
  stl_allocator &operator=(
      const stl_allocator<T2, manager_kernel_type> &other) noexcept {
    m_ptr_manager_kernel_address = other.m_ptr_manager_kernel_address;
    return *this;
  }

  /// \brief Move assign operator
  stl_allocator &operator=(stl_allocator &&other) noexcept = default;

  /// \brief Move assign operator for another T
  template <typename T2>
  stl_allocator &operator=(
      stl_allocator<T2, manager_kernel_type> &&other) noexcept {
    m_ptr_manager_kernel_address = other.m_ptr_manager_kernel_address;
    return *this;
  }

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const { return priv_allocate(n); }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const {
    return priv_deallocate(ptr, size);
  }

  /// \brief The size of the theoretical maximum allocation size
  /// \return The size of the theoretical maximum allocation size
  size_type max_size() const noexcept { return priv_max_size(); }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&...args) const {
    priv_construct(ptr, std::forward<Args>(args)...);
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const { priv_destroy(ptr); }

  // ---------- This class's unique public functions ---------- //
  /// \brief Returns a pointer that points to manager kernel
  /// \return A pointer that points to manager kernel
  manager_kernel_type *const *get_pointer_to_manager_kernel() const {
    return to_raw_pointer(m_ptr_manager_kernel_address);
  }

 private:
  // -------------------- //
  // Private methods
  // -------------------- //

  pointer priv_allocate(const size_type n) const {
    std::cerr << __FILE__ << " " << __LINE__ << std::endl;
    if (priv_max_size() < n) {
      throw std::bad_array_new_length();
    }
    std::cerr << __FILE__ << " " << __LINE__ << std::endl;
    if (!get_pointer_to_manager_kernel()) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "nullptr: cannot access to manager kernel");
      throw std::bad_alloc();
    }
    std::cerr << __FILE__ << " " << __LINE__ << std::endl;
    auto* manager_kernel = *get_pointer_to_manager_kernel();
    if (!manager_kernel) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "nullptr: cannot access to manager kernel");
      throw std::bad_alloc();
    }
    std::cerr << __FILE__ << " " << __LINE__ << " Allocate (bytes) " << n * sizeof(T) << std::endl;
    auto addr = pointer(
        static_cast<value_type *>(manager_kernel->allocate(n * sizeof(T))));
    std::cerr << __FILE__ << " " << __LINE__  << std::endl;
    if (!addr) {
      std::cerr << __FILE__ << " " << __LINE__ << std::endl;
      throw std::bad_alloc();
    }
    std::cerr << __FILE__ << " " << __LINE__ << std::endl;

    return addr;
  }

  void priv_deallocate(pointer ptr,
                       [[maybe_unused]] const size_type size) const noexcept {
    if (!get_pointer_to_manager_kernel()) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "nullptr: cannot access to manager kernel");
      return;
    }
    auto manager_kernel = *get_pointer_to_manager_kernel();
    if (!manager_kernel) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "nullptr: cannot access to manager kernel");
      return;
    }
    manager_kernel->deallocate(to_raw_pointer(ptr));
  }

  size_type priv_max_size() const noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }

  template <class... arg_types>
  void priv_construct(const pointer &ptr, arg_types &&...args) const {
    ::new ((void *)to_raw_pointer(ptr))
        value_type(std::forward<arg_types>(args)...);
  }

  void priv_destroy(const pointer &ptr) const {
    if (!ptr) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "pointer is nullptr");
    }
    (*ptr).~value_type();
  }

  // -------------------- //
  // Private fields
  // -------------------- //
 private:
  // (offset)pointer to a raw pointer that points a manager kernel object
  // allocated in DRAM i.e., offset_ptr<manager_kernel_type *const>
  typename std::pointer_traits<typename manager_kernel_type::void_pointer>::
      template rebind<manager_kernel_type *const>
          m_ptr_manager_kernel_address;
};

template <typename T, typename kernel>
inline bool operator==(const stl_allocator<T, kernel> &rhd,
                       const stl_allocator<T, kernel> &lhd) {
  // Return true if they point to the same manager kernel
  return rhd.get_pointer_to_manager_kernel() ==
         lhd.get_pointer_to_manager_kernel();
}

template <typename T, typename kernel>
inline bool operator!=(const stl_allocator<T, kernel> &rhd,
                       const stl_allocator<T, kernel> &lhd) {
  return !(rhd == lhd);
}

}  // namespace metall

#endif  // METALL_STL_ALLOCATOR_HPP
