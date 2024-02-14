// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_FALLBACK_ALLOCATOR_ADAPTOR_HPP
#define METALL_UTILITY_FALLBACK_ALLOCATOR_ADAPTOR_HPP

#include <memory>
#include <cstdlib>
#include <type_traits>

#include <metall/stl_allocator.hpp>

namespace metall::utility {

/// \brief A STL compatible allocator which fallbacks to a heap allocator (e.g.,
/// malloc) if its constructor receives no argument. \tparam primary_alloc A
/// primary allocator type, i.e., metall::stl_allocator.
///
/// \details
/// Using this allocator, the following code will work:
/// \code
/// {
///  using alloc =
///  fallback_allocator_adaptor<metall::manager::allocator_type<int>>;
///  vector<int, alloc> temp_vec; // Allocate an vector object temporarily in
///  'DRAM'.
/// }
/// \endcode
/// \attention
/// The purpose of this allocator is providing a way to quickly integrate Metall
/// into an application which wants to allocate 'metallized' classes temporarily
/// in 'DRAM' occasionally. This allocator could cause difficult bugs to debug.
/// Use this allocator with great care.
template <typename primary_alloc>
class fallback_allocator_adaptor {
 private:
  template <typename T>
  using other_primary_allocator_type =
      typename std::allocator_traits<primary_alloc>::template rebind_alloc<T>;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using primary_allocator_type = typename std::remove_const<
      typename std::remove_reference<primary_alloc>::type>::type;

  using value_type = typename primary_allocator_type::value_type;
  using pointer = typename primary_allocator_type::pointer;
  using const_pointer = typename primary_allocator_type::const_pointer;
  using void_pointer = typename primary_allocator_type::void_pointer;
  using const_void_pointer =
      typename primary_allocator_type::const_void_pointer;
  using difference_type = typename primary_allocator_type::difference_type;
  using size_type = typename primary_allocator_type::size_type;

  /// \brief Makes another allocator type for type T2
  template <typename T2>
  struct rebind {
    using other = fallback_allocator_adaptor<other_primary_allocator_type<T2>>;
  };

 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //

  /// \brief Default constructor which falls back on a heap allocator.
  fallback_allocator_adaptor() noexcept : m_primary_allocator(nullptr) {}

  /// \brief Construct a new instance using an instance of
  /// fallback_allocator_adaptor with any primary_allocator type.
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor(fallback_allocator_adaptor<primary_allocator_type2>
                                 allocator_instance) noexcept
      : m_primary_allocator(allocator_instance.primary_allocator()) {}

  /// \brief Construct a new instance using an instance of any
  /// primary_allocator.
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor(
      primary_allocator_type2 allocator_instance) noexcept
      : m_primary_allocator(allocator_instance) {}

  /// \brief Copy constructor
  fallback_allocator_adaptor(const fallback_allocator_adaptor &other) noexcept =
      default;

  /// \brief Move constructor
  fallback_allocator_adaptor(fallback_allocator_adaptor &&other) noexcept =
      default;

  /// \brief Copy assign operator
  fallback_allocator_adaptor &operator=(
      const fallback_allocator_adaptor &) noexcept = default;

  /// \brief Copy assign operator, using an instance of
  /// fallback_allocator_adaptor with any primary_allocator type
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor &operator=(
      const fallback_allocator_adaptor<primary_allocator_type2>
          &other) noexcept {
    m_primary_allocator = other.primary_allocator();
    return *this;
  }

  /// \brief Copy assign operator for any primary_allocator
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor &operator=(
      const primary_allocator_type2 &allocator_instance) noexcept {
    m_primary_allocator = allocator_instance;
    return *this;
  }

  /// \brief Move assign operator
  fallback_allocator_adaptor &operator=(
      fallback_allocator_adaptor &&other) noexcept = default;

  /// \brief Move assign operator, using an instance of
  /// fallback_allocator_adaptor with any primary_allocator type
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor &operator=(
      fallback_allocator_adaptor<primary_allocator_type2> &&other) noexcept {
    m_primary_allocator = std::move(other.primary_allocator());
    return *this;
  }

  /// \brief Move assign operator for any primary_allocator
  template <typename primary_allocator_type2,
            std::enable_if_t<std::is_constructible<
                                 primary_alloc, primary_allocator_type2>::value,
                             int> = 0>
  fallback_allocator_adaptor &operator=(
      primary_allocator_type2 &&allocator_instance) noexcept {
    m_primary_allocator = std::move(allocator_instance);
    return *this;
  }

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    if (priv_primary_allocator_available()) {
      return m_primary_allocator.allocate(n);
    }
    return priv_fallback_allocate(n);
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const {
    if (priv_primary_allocator_available()) {
      m_primary_allocator.deallocate(ptr, size);
    } else {
      priv_fallback_deallocate(ptr);
    }
  }

  /// \brief The size of the theoretical maximum allocation size
  /// \return The size of the theoretical maximum allocation size
  size_type max_size() const noexcept { return m_primary_allocator.max_size(); }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&...args) const {
    if (priv_primary_allocator_available()) {
      m_primary_allocator.construct(ptr, std::forward<Args>(args)...);
    } else {
      priv_fallback_construct(ptr, std::forward<Args>(args)...);
    }
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const {
    if (priv_primary_allocator_available()) {
      m_primary_allocator.destroy(ptr);
    } else {
      priv_fallback_destroy(ptr);
    }
  }

  // ---------- This class's unique public functions ---------- //
  primary_allocator_type &primary_allocator() { return m_primary_allocator; }

  const primary_allocator_type &primary_allocator() const {
    return m_primary_allocator;
  }

 private:
  // -------------------- //
  // Private methods
  // -------------------- //
  auto priv_primary_allocator_available() const {
    return !!(m_primary_allocator.get_pointer_to_manager_kernel());
  }

  pointer priv_fallback_allocate(const size_type n) const {
    if (max_size() < n) {
      throw std::bad_array_new_length();
    }

    void *const addr = std::malloc(n * sizeof(value_type));
    if (!addr) {
      throw std::bad_alloc();
    }

    return pointer(static_cast<value_type *>(addr));
  }

  void priv_fallback_deallocate(pointer ptr) const {
    std::free(to_raw_pointer(ptr));
  }

  void priv_fallback_destroy(pointer ptr) const { (*ptr).~value_type(); }

  template <class... arg_types>
  void priv_fallback_construct(const pointer &ptr, arg_types &&...args) const {
    ::new ((void *)to_raw_pointer(ptr))
        value_type(std::forward<arg_types>(args)...);
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  primary_allocator_type m_primary_allocator;
};

template <typename primary_allocator_type>
inline bool operator==(
    const fallback_allocator_adaptor<primary_allocator_type> &rhd,
    const fallback_allocator_adaptor<primary_allocator_type> &lhd) {
  // Return true if they point to the same manager kernel
  return rhd.primary_allocator() == lhd.primary_allocator();
}

template <typename primary_allocator_type>
inline bool operator!=(
    const fallback_allocator_adaptor<primary_allocator_type> &rhd,
    const fallback_allocator_adaptor<primary_allocator_type> &lhd) {
  return !(rhd == lhd);
}

}  // namespace metall::utility

/// \example fallback_allocator_adaptor.cpp
/// This is an example of how to use fallback_allocator_adaptor.

#endif  // METALL_UTILITY_FALLBACK_ALLOCATOR_ADAPTOR_HPP
