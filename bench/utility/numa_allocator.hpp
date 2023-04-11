// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_NUMA_ALLOCATOR_HPP
#define METALL_BENCH_UTILITY_NUMA_ALLOCATOR_HPP

#include <memory>
#include "numa.hpp"

namespace bench_utility::numa {

template <typename T>
class numa_allocator {
 public:
  // ---------- Types ---------- //
  using value_type = T;
  using pointer = T *;
  using size_type = std::size_t;

  // ---------- Constructor ---------- //
  numa_allocator() = default;

  /// \brief Construct a new instance using an instance that has a different T
  template <typename T2>
  numa_allocator(const numa_allocator<T2> &){};

  // ---------- Copy and move constructor ---------- //
  numa_allocator(const numa_allocator &other) = default;
  numa_allocator(numa_allocator &&other) noexcept = default;

  // ---------- Copy and move assignments ---------- //
  numa_allocator &operator=(const numa_allocator &) = default;
  numa_allocator &operator=(numa_allocator &&other) noexcept = default;

  template <typename T2>
  numa_allocator &operator=([[maybe_unused]] const numa_allocator<T2> &other) {
    return *this;
  }

  template <typename T2>
  numa_allocator &operator=(
      [[maybe_unused]] numa_allocator<T2> &&other) noexcept {
    return *this;
  }

  /// \brief Makes another allocator type for type T2
  /// \tparam T2 The type of the object
  template <typename T2>
  struct rebind {
    using other = numa_allocator<T2>;
  };

  /// \brief Allocates n * sizeof(T) bytes of storage on the local numa node
  /// \param n The size to allocation
  /// \return Returns a pointer
  pointer allocate(const size_type n) const {
    return pointer(numa::alloc_local(n * sizeof(T)));
  }

  /// \brief Deallocates the storage reference by the pointer ptr
  /// \param ptr A pointer to the storage
  /// \param size The size of the storage
  void deallocate(pointer ptr, const size_type size) const noexcept {
    numa::free(ptr, size);
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

  numa_allocator<T> select_on_container_copy_construction() const {
    return numa_allocator<T>(*this);
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
bool operator==([[maybe_unused]] const numa_allocator<T> &rhd,
                [[maybe_unused]] const numa_allocator<T> &lhd) {
  return true;
}

template <typename T>
bool operator!=(const numa_allocator<T> &rhd, const numa_allocator<T> &lhd) {
  return !(rhd == lhd);
}

}  // namespace bench_utility::numa
#endif  // METALL_UTILITY_NUMA_ALLOCATOR_HPP
