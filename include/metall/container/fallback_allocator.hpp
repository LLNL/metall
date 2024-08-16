// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_FALLBACK_ALLOCATOR_HPP
#define METALL_CONTAINER_FALLBACK_ALLOCATOR_HPP

#include <memory>
#include <cstdlib>
#include <type_traits>

namespace metall::container {

/// \brief A STL compatible allocator which fallbacks to a heap allocator (e.g.,
/// malloc()) if its constructor receives no argument to construct the stateful
/// allocator instance.
/// \tparam stateful_allocator The stateful allocator type. It must not be
/// default constructible.
template <typename stateful_allocator>
class fallback_allocator_adaptor {
  // Check if the stateful_allocator takes arugments in its constructor
  static_assert(!std::is_constructible<stateful_allocator>::value,
                "The stateful allocator must not be default constructible");

 private:
  template <typename T>
  using other_stateful_allocatorator_type = typename std::allocator_traits<
      stateful_allocator>::template rebind_alloc<T>;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using stateful_allocatorator_type = typename std::remove_const<
      typename std::remove_reference<stateful_allocator>::type>::type;

  using value_type = typename stateful_allocatorator_type::value_type;
  using pointer = typename stateful_allocatorator_type::pointer;
  using const_pointer = typename stateful_allocatorator_type::const_pointer;
  using void_pointer = typename stateful_allocatorator_type::void_pointer;
  using const_void_pointer =
      typename stateful_allocatorator_type::const_void_pointer;
  using difference_type = typename stateful_allocatorator_type::difference_type;
  using size_type = typename stateful_allocatorator_type::size_type;

  /// \brief Makes another allocator type for type T2
  template <typename T2>
  struct rebind {
    using other =
        fallback_allocator_adaptor<other_stateful_allocatorator_type<T2>>;
  };

 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //

  /// \brief Default constructor which falls back on the regular allocator
  /// (i.e., malloc()).
  fallback_allocator_adaptor() noexcept : m_stateful_allocatorator(nullptr) {}

  /// \brief Construct a new instance using an instance of
  /// fallback_allocator_adaptor with any stateful_allocatorator type.
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor(
      fallback_allocator_adaptor<stateful_allocatorator_type2>
          allocator_instance) noexcept
      : m_stateful_allocatorator(allocator_instance.stateful_allocatorator()) {}

  /// \brief Construct a new instance using an instance of any
  /// stateful_allocatorator.
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor(
      stateful_allocatorator_type2 allocator_instance) noexcept
      : m_stateful_allocatorator(allocator_instance) {}

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
  /// fallback_allocator_adaptor with any stateful_allocatorator type
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor &operator=(
      const fallback_allocator_adaptor<stateful_allocatorator_type2>
          &other) noexcept {
    m_stateful_allocatorator = other.stateful_allocatorator();
    return *this;
  }

  /// \brief Copy assign operator for any stateful_allocatorator
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor &operator=(
      const stateful_allocatorator_type2 &allocator_instance) noexcept {
    m_stateful_allocatorator = allocator_instance;
    return *this;
  }

  /// \brief Move assign operator
  fallback_allocator_adaptor &operator=(
      fallback_allocator_adaptor &&other) noexcept = default;

  /// \brief Move assign operator, using an instance of
  /// fallback_allocator_adaptor with any stateful_allocatorator type
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor &operator=(
      fallback_allocator_adaptor<stateful_allocatorator_type2>
          &&other) noexcept {
    m_stateful_allocatorator = std::move(other.stateful_allocatorator());
    return *this;
  }

  /// \brief Move assign operator for any stateful_allocatorator
  template <typename stateful_allocatorator_type2,
            std::enable_if_t<
                std::is_constructible<stateful_allocator,
                                      stateful_allocatorator_type2>::value,
                int> = 0>
  fallback_allocator_adaptor &operator=(
      stateful_allocatorator_type2 &&allocator_instance) noexcept {
    m_stateful_allocatorator = std::move(allocator_instance);
    return *this;
  }

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    if (priv_stateful_allocatorator_available()) {
      return m_stateful_allocatorator.allocate(n);
    }
    return priv_fallback_allocate(n);
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const {
    if (priv_stateful_allocatorator_available()) {
      m_stateful_allocatorator.deallocate(ptr, size);
    } else {
      priv_fallback_deallocate(ptr);
    }
  }

  /// \brief The size of the theoretical maximum allocation size
  /// \return The size of the theoretical maximum allocation size
  size_type max_size() const noexcept {
    return m_stateful_allocatorator.max_size();
  }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&...args) const {
    if (priv_stateful_allocatorator_available()) {
      m_stateful_allocatorator.construct(ptr, std::forward<Args>(args)...);
    } else {
      priv_fallback_construct(ptr, std::forward<Args>(args)...);
    }
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const {
    if (priv_stateful_allocatorator_available()) {
      m_stateful_allocatorator.destroy(ptr);
    } else {
      priv_fallback_destroy(ptr);
    }
  }

  // ---------- This class's unique public functions ---------- //

  /// \brief Returns a reference to the stateful allocatorator.
  stateful_allocatorator_type &stateful_allocatorator() {
    return m_stateful_allocatorator;
  }

  /// \brief Returns a const reference to the stateful allocatorator.
  const stateful_allocatorator_type &stateful_allocatorator() const {
    return m_stateful_allocatorator;
  }

  /// \brief Returns true if the stateful allocatorator is available.
  /// \return Returns true if the stateful allocatorator is available.
  bool stateful_allocatorator_available() const {
    return priv_stateful_allocatorator_available();
  }

 private:
  // -------------------- //
  // Private methods
  // -------------------- //
  auto priv_stateful_allocatorator_available() const {
    return !!(m_stateful_allocatorator.get_pointer_to_manager_kernel());
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
  stateful_allocatorator_type m_stateful_allocatorator;
};

template <typename stateful_allocatorator_type>
inline bool operator==(
    const fallback_allocator_adaptor<stateful_allocatorator_type> &rhd,
    const fallback_allocator_adaptor<stateful_allocatorator_type> &lhd) {
  // Return true if they point to the same manager kernel
  return rhd.stateful_allocatorator() == lhd.stateful_allocatorator();
}

template <typename stateful_allocatorator_type>
inline bool operator!=(
    const fallback_allocator_adaptor<stateful_allocatorator_type> &rhd,
    const fallback_allocator_adaptor<stateful_allocatorator_type> &lhd) {
  return !(rhd == lhd);
}

}  // namespace metall::container

/// \example fallback_allocator.cpp
/// This is an example of how to use fallback_allocator.

#endif  // METALL_CONTAINER_FALLBACK_ALLOCATOR_HPP
