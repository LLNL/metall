// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_JEMALLOC_STL_ALLOCATOR_HPP
#define METALL_BENCH_UTILITY_JEMALLOC_STL_ALLOCATOR_HPP

#include <jemalloc/jemalloc.h>

namespace bench_utility {

/// \brief A STL compatible allocator
/// \tparam T The type of the object
template <typename T>
class jemalloc_allocator {
 public:
  // ---------- Types ---------- //
  using value_type = T;
  using pointer = T *;
  using size_type = std::size_t;

  // ---------- Constructor ---------- //
  jemalloc_allocator() = default;

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  jemalloc_allocator(const jemalloc_allocator<T2> &allocator_instance){};

  // ---------- Copy and move constructor ---------- //
  jemalloc_allocator(const jemalloc_allocator &other) = default;
  jemalloc_allocator(jemalloc_allocator &&other) noexcept = default;

  // ---------- Copy and move assignments ---------- //
  jemalloc_allocator &operator=(const jemalloc_allocator &) = default;
  jemalloc_allocator &operator=(jemalloc_allocator &&other) noexcept = default;

  template <typename T2>
  jemalloc_allocator &operator=(const jemalloc_allocator<T2> &other) {
    return *this;
  }

  template <typename T2>
  jemalloc_allocator &operator=(jemalloc_allocator<T2> &&other) noexcept {
    return *this;
  }

  /// \brief Makes another allocator type for type T2
  /// \tparam T2 The type of the object
  template <typename T2>
  struct rebind {
    using other = jemalloc_allocator<T2>;
  };

  /// \brief Allocates n * sizeof(T) bytes of storage
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    return pointer(::je_malloc(n * sizeof(T)));
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const noexcept {
    ::je_free(ptr);
  }

  // TODO: Implement
  // size_type max_size() const noexcept {
  // }

  /// \brief Constructs an object of T
  /// \tparam Args The types of the constructor arguments
  /// \param ptr A pointer to allocated storage
  /// \param args The constructor arguments to use
  template <class... Args>
  void construct(const pointer &ptr, Args &&...args) const {
    ::new ((void *)(ptr)) value_type(std::forward<Args>(args)...);
  }

  /// \brief Deconstruct an object of T
  /// \param ptr A pointer to the object
  void destroy(const pointer &ptr) const {
    assert(ptr != 0);
    (*ptr).~value_type();
  }

  jemalloc_allocator<T> select_on_container_copy_construction() const {
    return jemalloc_allocator<T>(*this);
  }

  bool propagate_on_container_copy_assignment() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_move_assignment() const noexcept {
    return std::true_type();
  }

  bool propagate_on_container_swap() const noexcept { return std::true_type(); }
};

template <typename T>
inline bool operator==(const jemalloc_allocator<T> &rhd,
                       const jemalloc_allocator<T> &lhd) {
  return true;
}

template <typename T>
inline bool operator!=(const jemalloc_allocator<T> &rhd,
                       const jemalloc_allocator<T> &lhd) {
  return !(rhd == lhd);
}

}  // namespace bench_utility

#endif  // METALL_BENCH_UTILITY_JEMALLOC_STL_ALLOCATOR_HPP
