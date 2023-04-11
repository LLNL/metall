// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_OBJECT_SIZE_MANAGER_HPP
#define METALL_DETAIL_KERNEL_OBJECT_SIZE_MANAGER_HPP

#include <cstddef>
#include <type_traits>
#include <array>

#include <metall/detail/utilities.hpp>
#include <metall/detail/builtin_functions.hpp>

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

namespace object_size_manager_detail {

// Sizes are coming from SuperMalloc
constexpr std::size_t k_class1_small_size_table[] = {
    8,  10, 12, 14, 16,  20,  24,  28,  32,  40,  48,
    56, 64, 80, 96, 112, 128, 160, 192, 224, 256,
};

constexpr uint64_t k_num_class1_small_sizes =
    (uint64_t)std::extent<decltype(k_class1_small_size_table)>::value;
constexpr std::size_t k_min_class2_offset = 64;
template <std::size_t k_chunk_size>
constexpr std::size_t k_max_small_size = k_chunk_size / 2;

// Sizes are coming from jemalloc
template <std::size_t k_chunk_size>
inline constexpr uint64_t num_class2_small_sizes() noexcept {
  std::size_t size = k_class1_small_size_table[k_num_class1_small_sizes - 1];
  uint64_t num_class2_small_sizes = 0;
  uint64_t offset = k_min_class2_offset;

  while (size <= k_max_small_size<k_chunk_size>) {
    for (int i = 0; i < 4; ++i) {
      size += offset;
      if (size > k_max_small_size<k_chunk_size>) break;
      ++num_class2_small_sizes;
    }
    offset *= 2;
  }

  return num_class2_small_sizes;
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
inline constexpr uint64_t num_large_sizes() noexcept {
  uint64_t count = 0;
  for (std::size_t size = k_chunk_size; size <= k_max_size; size *= 2) {
    ++count;
  }
  return count;
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr uint64_t k_num_sizes =
    k_num_class1_small_sizes + num_class2_small_sizes<k_chunk_size>() +
    num_large_sizes<k_chunk_size, k_max_size>();

template <std::size_t k_chunk_size, std::size_t k_max_size>
inline constexpr std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>>
init_size_table() noexcept {
  // MEMO: {} is needed to prevent the uninitialized error in constexpr
  // contexts. This technique is not needed from C++20.
  std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>> table{};

  uint64_t index = 0;

  for (; index < k_num_class1_small_sizes; ++index) {
    table[index] = k_class1_small_size_table[index];
  }

  {
    std::size_t size = k_class1_small_size_table[k_num_class1_small_sizes - 1];
    uint64_t offset = k_min_class2_offset;
    while (size <= k_max_small_size<k_chunk_size>) {
      for (int i = 0; i < 4; ++i) {
        size += offset;
        if (size > k_max_small_size<k_chunk_size>) break;
        table[index] = size;
        ++index;
      }
      offset *= 2;
    }
  }

  {
    std::size_t size = k_chunk_size;
    for (uint64_t i = 0; i < num_large_sizes<k_chunk_size, k_max_size>(); ++i) {
      table[index] = size;
      size *= 2;
      ++index;
    }
  }

  return table;
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>>
    k_size_table = init_size_table<k_chunk_size, k_max_size>();

template <std::size_t k_chunk_size, std::size_t k_max_size>
inline constexpr int64_t find_in_size_table(
    const std::size_t size, const uint64_t offset = 0) noexcept {
  for (uint64_t i = offset; i < k_size_table<k_chunk_size, k_max_size>.size();
       ++i) {
    if (size <= k_size_table<k_chunk_size, k_max_size>[i])
      return static_cast<int64_t>(i);
  }
  return -1;  // Error
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
inline constexpr int64_t object_size_index(const std::size_t size) noexcept {
  if (size <= k_size_table<k_chunk_size, k_max_size>[0]) return 0;

  if (size <= k_class1_small_size_table[k_num_class1_small_sizes - 1]) {
    const int z = mdtl::clzll(size);
    const std::size_t r = size + (1ULL << (61ULL - z)) - 1;
    const int y = mdtl::clzll(r);
    const int index =
        static_cast<int>(4 * (60 - y) + ((r >> (61ULL - y)) & 3ULL));
    return static_cast<int64_t>(index);
  }

  return find_in_size_table<k_chunk_size, k_max_size>(size,
                                                      k_num_class1_small_sizes);
}

}  // namespace object_size_manager_detail

namespace {
namespace dtl = object_size_manager_detail;
}

/// \brief Object size manager
template <std::size_t k_chunk_size, std::size_t k_max_object_size>
class object_size_manager {
 public:
  using size_type = std::size_t;

 public:
  object_size_manager() = delete;
  ~object_size_manager() = delete;
  object_size_manager(const object_size_manager &) = delete;
  object_size_manager(object_size_manager &&) noexcept = delete;
  object_size_manager &operator=(const object_size_manager &) = delete;
  object_size_manager &operator=(object_size_manager &&) noexcept = delete;

  static constexpr size_type at(const size_type i) noexcept {
    return dtl::k_size_table<k_chunk_size, k_max_object_size>[i];
  }

  static constexpr size_type num_sizes() noexcept {
    return dtl::k_num_sizes<k_chunk_size, k_max_object_size>;
  }

  static constexpr size_type num_small_sizes() noexcept {
    return dtl::k_num_class1_small_sizes +
           dtl::num_class2_small_sizes<k_chunk_size>();
  }

  static constexpr size_type num_large_sizes() noexcept {
    return dtl::num_large_sizes<k_chunk_size, k_max_object_size>();
  }

  static constexpr int64_t index(const size_type size) noexcept {
    return dtl::object_size_index<k_chunk_size, k_max_object_size>(size);
  }
};

}  // namespace kernel
}  // namespace metall
#endif  // METALL_DETAIL_KERNEL_OBJECT_SIZE_MANAGER_HPP
