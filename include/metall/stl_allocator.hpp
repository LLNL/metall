// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)
#ifndef METALL_STL_ALLOCATOR_HPP
#define METALL_STL_ALLOCATOR_HPP

#include <memory>
#include <type_traits>
#include <cassert>
#include <array>
#include <limits>

#include <metall/offset_ptr.hpp>

namespace metall {
namespace detail {
std::size_t g_max_manager_kernel_id;

template <typename manager_kernel_type>
manager_kernel_type** manager_kernel_table(typename manager_kernel_type::id_type id) {
  static std::array<manager_kernel_type*, 1024> table;
  assert(id < 1024);
  assert(id <= g_max_manager_kernel_id);
  return &(table[id]);
}

} // namespace detail
} // namespace metall

namespace metall {

/// \brief A STL compatible allocator
/// \tparam T The type of the object
/// \tparam manager_kernel_type The type of the manager kernel
template <typename T, typename manager_kernel_type>
class stl_allocator {
 private:
  using kernel_id_type = typename manager_kernel_type::id_type;

 public:
  // -------------------- Types -------------------- //
  using value_type = T;
  using pointer = typename std::pointer_traits<typename manager_kernel_type::void_pointer>::template rebind<value_type>;
  using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
  using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
  using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
  using difference_type = typename std::pointer_traits<pointer>::difference_type;
  using size_type = typename std::make_unsigned<difference_type>::type;

  // -------------------- Constructor -------------------- //
  // Note: same as manager.hpp in Boost.interprocess,
  // 'explicit' keyword is not used on purpose to enable to call this constructor w/o arguments
  stl_allocator(kernel_id_type kernel_id)
      : m_kernel_id(kernel_id) {}

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  stl_allocator(const stl_allocator<T2, manager_kernel_type> &allocator_instance)
    : m_kernel_id(allocator_instance.get_kernel_id()) {}

  // -------------------- Copy and move constructor -------------------- //
  stl_allocator(const stl_allocator<T, manager_kernel_type> &other) = default;
  stl_allocator(stl_allocator<T, manager_kernel_type> &&other) noexcept = default;

  // -------------------- Copy and move assignments -------------------- //
  stl_allocator &operator=(const stl_allocator<T, manager_kernel_type> &) = default;
  stl_allocator &operator=(stl_allocator<T, manager_kernel_type> &&other) noexcept  = default;

  template <typename T2>
  stl_allocator &operator=(const stl_allocator<T2, manager_kernel_type> &other) {
    m_kernel_id = other.m_kernel_id;
    return *this;
  }

  template <typename T2>
  stl_allocator &operator=(stl_allocator<T2, manager_kernel_type> &&other) noexcept {
    m_kernel_id = other.m_kernel_id;
    return *this;
  }

  /// \brief Makes another allocator type for type T2
  /// \tparam T2 The type of the object
  template <typename T2>
  struct rebind {
    using other = stl_allocator<T2, manager_kernel_type>;
  };

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    manager_kernel_type *const kernel = *(detail::manager_kernel_table<manager_kernel_type>(m_kernel_id));
    return pointer(static_cast<value_type*>(kernel->allocate(n * sizeof(T))));
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const noexcept {
    manager_kernel_type *const kernel = *(detail::manager_kernel_table<manager_kernel_type>(m_kernel_id));
    kernel->deallocate(to_raw_pointer(ptr));
  }

  // TODO: Implement
  // size_type max_size() const noexcept {
  // }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&... args) const {
    ::new((void *)to_raw_pointer(ptr)) value_type(std::forward<Args>(args)...);
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const {
    assert(ptr != 0);
    (*ptr).~value_type();
  }

  stl_allocator<T, manager_kernel_type> select_on_container_copy_construction() const {
    return stl_allocator<T, manager_kernel_type>(*this);
  }

  bool propagate_on_container_copy_assignment() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_move_assignment() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_swap() const noexcept {
    return std::true_type();
  }

  kernel_id_type get_kernel_id() const {
    return m_kernel_id;
  }

 private:
  // Initialize with a large value to detect uninitialized situations
  kernel_id_type m_kernel_id {std::numeric_limits<kernel_id_type>::max()};

};

template <typename T, typename kernel>
bool operator==(const stl_allocator<T, kernel> &rhd, const stl_allocator<T, kernel> &lhd) {
  return rhd.get_kernel_id() == lhd.get_kernel_id();
}

template <typename T, typename kernel>
bool operator!=(const stl_allocator<T, kernel> &rhd, const stl_allocator<T, kernel> &lhd) {
  return !(rhd == lhd);
}

} // namespace metall

#endif //METALL_STL_ALLOCATOR_HPP
