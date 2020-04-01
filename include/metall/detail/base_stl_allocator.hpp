// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_DETAIL_BASE_STL_ALLOCATOR_HPP
#define METALL_DETAIL_BASE_STL_ALLOCATOR_HPP

#include <memory>
#include <type_traits>
#include <cassert>
#include <array>
#include <limits>

#include <metall/offset_ptr.hpp>

namespace metall::detail {

/// \brief A STL compatible allocator designed to act as a base class of CRTP
/// \tparam impl_type A class of actual implementation
/// \tparam type_holder An utility class that holds important types
template <typename impl_type, typename type_holder>
class base_stl_allocator {

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using pointer = typename type_holder::pointer;
  using const_pointer = typename type_holder::const_pointer;
  using void_pointer = typename type_holder::void_pointer;
  using const_void_pointer = typename type_holder::const_void_pointer;
  using value_type = typename type_holder::value_type;
  using size_type = typename type_holder::size_type;
  using difference_type = typename type_holder::difference_type;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  base_stl_allocator() = default;
  ~base_stl_allocator() = default;

  base_stl_allocator(const base_stl_allocator &other) = default;
  base_stl_allocator(base_stl_allocator &&other) = default;

  base_stl_allocator &operator=(const base_stl_allocator &) = default;
  base_stl_allocator &operator=(base_stl_allocator &&other) = default;

  /// \brief Makes another allocator type for type T2
  /// \tparam T2 The type of the object
  template <typename T2>
  struct rebind {
    using other = typename impl_type::template rebind_impl<T2>::other;
  };

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    return impl().allocate_impl(n);
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const noexcept {
    return impl().deallocate_impl(ptr, size);
  }

  /// \brief The size of the theoretical maximum allocation size
  /// \return The size of the theoretical maximum allocation size
  size_type max_size() const noexcept {
    return impl().max_size_impl();
  }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&... args) const {
    impl().construct_impl(ptr, std::forward<Args>(args)...);
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const {
    impl().destroy_impl(ptr);
  }

  base_stl_allocator select_on_container_copy_construction() const {
    return impl().select_on_container_copy_construction_impl();
  }

  bool propagate_on_container_copy_assignment() const noexcept {
    return impl().propagate_on_container_copy_assignment_impl();
  }

  bool propagate_on_container_move_assignment() const noexcept {
    return impl().propagate_on_container_move_assignment_impl();
  }

  bool propagate_on_container_swap() const noexcept {
    return impl().propagate_on_container_swap_impl();
  }

  bool is_always_equal() const noexcept {
    return impl().is_always_equal_impl();
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  impl_type &impl() { return *static_cast<impl_type *>(this); }
  const impl_type &impl() const { return *static_cast<const impl_type *>(this); }

};

template <typename impl_type, typename type_holder>
bool operator==(const base_stl_allocator<impl_type, type_holder> &rhd,
                const base_stl_allocator<impl_type, type_holder> &lhd) {
  auto& rhd_impl = static_cast<const impl_type&>(rhd);
  auto& lhd_impl = static_cast<const impl_type&>(lhd);
  return rhd_impl == lhd_impl;
}

template <typename impl_type, typename type_holder>
bool operator!=(const base_stl_allocator<impl_type, type_holder> &rhd,
                const base_stl_allocator<impl_type, type_holder> &lhd) {
  auto& rhd_impl = static_cast<const impl_type&>(rhd);
  auto& lhd_impl = static_cast<const impl_type&>(lhd);
  return rhd_impl != lhd_impl;
}

} // namespace metall::detail

#endif //METALL_DETAIL_BASE_STL_ALLOCATOR_HPP
