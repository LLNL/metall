//
// Created by Iwabuchi, Keita on 2019-05-19.
//

/// \file

#ifndef METALL_STL_ALLOCATOR_HPP
#define METALL_STL_ALLOCATOR_HPP

#include <memory>
#include <type_traits>
#include <cassert>
#include <limits>

#include <metall/detail/base_stl_allocator.hpp>
#include <metall/offset_ptr.hpp>

namespace metall::detail {

/// \brief This is a utility structure that declare and holds some impormant types used in stl_allocator
/// \tparam T The type of the object
/// \tparam manager_kernel_type The type of the manager kernel
template <typename T, typename manager_kernel_type>
struct stl_allocator_type_holder {
  using value_type = T;
  using pointer = typename std::pointer_traits<typename manager_kernel_type::void_pointer>::template rebind<value_type>;
  using const_pointer = typename std::pointer_traits<pointer>::template rebind<const value_type>;
  using void_pointer = typename std::pointer_traits<pointer>::template rebind<void>;
  using const_void_pointer = typename std::pointer_traits<pointer>::template rebind<const void>;
  using difference_type = typename std::pointer_traits<pointer>::difference_type;
  using size_type = typename std::make_unsigned<difference_type>::type;
};

} // namespace metall::detail


namespace metall {

/// \brief A STL compatible allocator
/// \tparam T The type of the object
/// \tparam manager_kernel_type The type of the manager kernel
template <typename T, typename manager_kernel_type>
class stl_allocator
    : public metall::detail::base_stl_allocator<stl_allocator<T, manager_kernel_type>,
                                                detail::stl_allocator_type_holder<T, manager_kernel_type>> {

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = stl_allocator<T, manager_kernel_type>;
  using type_holder = detail::stl_allocator_type_holder<T, manager_kernel_type>;
  using base_type = metall::detail::base_stl_allocator<self_type, type_holder>;
  friend base_type;

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
  // Note: following manager.hpp in Boost.interprocess, 'explicit' keyword is not used on purpose
  // although this allocator won't work correctly w/o a valid manager_kernel_address
  stl_allocator(manager_kernel_type **const pointer_manager_kernel_address)
      : m_ptr_manager_kernel_address(pointer_manager_kernel_address) {
    assert(m_ptr_manager_kernel_address);
    assert(*m_ptr_manager_kernel_address);
  }

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  stl_allocator(const stl_allocator<T2, manager_kernel_type> &allocator_instance)
      : m_ptr_manager_kernel_address(allocator_instance.get_pointer_to_manager_kernel()) {
    assert(m_ptr_manager_kernel_address);
    assert(*m_ptr_manager_kernel_address);
  }

  stl_allocator(const stl_allocator<T, manager_kernel_type> &other) = default;
  stl_allocator(stl_allocator<T, manager_kernel_type> &&other) = default;

  stl_allocator &operator=(const stl_allocator<T, manager_kernel_type> &) = default;
  stl_allocator &operator=(stl_allocator<T, manager_kernel_type> &&other) = default;

  /// \brief Copy assign operator for another T
  template <typename T2>
  stl_allocator &operator=(const stl_allocator<T2, manager_kernel_type> &other) {
    m_ptr_manager_kernel_address = other.m_ptr_manager_kernel_address;
    assert(m_ptr_manager_kernel_address);
    assert(*m_ptr_manager_kernel_address);
    return *this;
  }

  /// \brief Move assign operator for another T
  template <typename T2>
  stl_allocator &operator=(stl_allocator<T2, manager_kernel_type> &&other) noexcept {
    m_ptr_manager_kernel_address = other.m_ptr_manager_kernel_address;
    assert(m_ptr_manager_kernel_address);
    assert(*m_ptr_manager_kernel_address);
    return *this;
  }

  // ----------------------------------- This class's unique public functions ----------------------------------- //
  /// \brief Returns a pointer that points to manager kernel
  /// \return A pointer that points to manager kernel
  manager_kernel_type **get_pointer_to_manager_kernel() const {
    assert(m_ptr_manager_kernel_address);
    assert(*m_ptr_manager_kernel_address);
    return metall::to_raw_pointer(m_ptr_manager_kernel_address);
  }

 private:
  /// -------------------------------------------------------------------------------- ///
  /// Private methods (required by the base class)
  /// -------------------------------------------------------------------------------- ///
  template <typename T2>
  struct rebind_impl {
    using other = stl_allocator<T2, manager_kernel_type>;
  };

  pointer allocate_impl(const size_type n) const {
    auto manager_kernel = *get_pointer_to_manager_kernel();
    assert(manager_kernel);
    return pointer(static_cast<value_type *>(manager_kernel->allocate(n * sizeof(T))));
  }

  void deallocate_impl(pointer ptr, [[maybe_unused]] const size_type size) const noexcept {
    auto manager_kernel = *get_pointer_to_manager_kernel();
    assert(manager_kernel);
    manager_kernel->deallocate(to_raw_pointer(ptr));
  }

  size_type max_size_impl() const noexcept {
    return std::numeric_limits<size_type>::max();
  }

  template <class... Args>
  void construct_impl(const pointer &ptr, Args &&... args) const {
    ::new((void *)to_raw_pointer(ptr)) value_type(std::forward<Args>(args)...);
  }

  void destroy_impl(const pointer &ptr) const {
    assert(ptr != 0);
    (*ptr).~value_type();
  }

  stl_allocator<T, manager_kernel_type> select_on_container_copy_construction_impl() const {
    return stl_allocator<T, manager_kernel_type>(*this);
  }

  bool propagate_on_container_copy_assignment_impl() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_move_assignment_impl() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_swap_impl() const noexcept {
    return std::true_type();
  }

  bool is_always_equal_impl() const noexcept {
    return std::false_type();
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
 private:
  // (offset)pointer to a raw pointer that points the address of the manager kernel allocated in DRAM
  typename std::pointer_traits<typename manager_kernel_type::void_pointer>::template rebind<manager_kernel_type *>
      m_ptr_manager_kernel_address;
};

template <typename T, typename kernel>
inline bool operator==(const stl_allocator<T, kernel> &rhd, const stl_allocator<T, kernel> &lhd) {
  // Return true if they point to the same manager kernel
  return *(rhd.get_pointer_to_manager_kernel()) == *(lhd.get_pointer_to_manager_kernel());
}

template <typename T, typename kernel>
inline bool operator!=(const stl_allocator<T, kernel> &rhd, const stl_allocator<T, kernel> &lhd) {
  return !(rhd == lhd);
}

}

#endif //METALL_STL_ALLOCATOR_HPP
