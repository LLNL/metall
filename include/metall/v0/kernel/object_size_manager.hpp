// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_OBJECT_SIZE_MANAGER_HPP
#define METALL_DETAIL_V0_KERNEL_OBJECT_SIZE_MANAGER_HPP

#include <cstddef>
#include <type_traits>

#include <metall/detail/utility/common.hpp>

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

namespace object_size_manager_detail {
constexpr std::size_t k_page_size = 4096;

// CAUTION:
// assume that page size is 4 KiB and chunk size is less than 1 GiB
constexpr std::size_t k_class1_small_size_table[] = {
    // Class 1 (small class in SuperMalloc): limit internal fragmentation to at most 25%.
    8, 10, 12, 14, 16, 20, 24, 28, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224,
    // Class 2 (medium class in SuperMalloc): a multiple of 64.
    256, 320, 448, 512, 576, 704, 960, 1024, 1216, 1472, 1984, 2048, 2752, 3904, 4096, 5312, 7232, 8192, 10048, 14272,
};
constexpr uint32_t k_class1_small_size_table_length = (uint32_t)std::extent<decltype(k_class1_small_size_table)>::value - 1;
constexpr std::size_t k_min_class2_small_size = (std::size_t)util::round_up((int64_t)k_class1_small_size_table[k_class1_small_size_table_length - 1] + 1, k_page_size);

constexpr uint32_t num_class1_small_sizes(const std::size_t chunk_size) noexcept {
  if (chunk_size <= k_class1_small_size_table[0]) return 0;

  for (uint32_t i = 0; i < k_class1_small_size_table_length; ++i) {
    if (chunk_size <= k_class1_small_size_table[i])
      return i;
  }

  return k_class1_small_size_table_length;
}

constexpr uint32_t num_class2_small_sizes(const std::size_t chunk_size) noexcept {
  std::size_t size = k_min_class2_small_size;
  uint32_t um_class1_small_sizes = 0;
  while (size <= chunk_size / 2) {
    size *= 2;
    ++um_class1_small_sizes;
  }

  return um_class1_small_sizes;
}

constexpr uint32_t num_large_sizes(const std::size_t chunk_size, const std::size_t max_size) noexcept {
  uint32_t count = 0;
  for (std::size_t size = chunk_size; size <= max_size; size *= 2) {
    ++count;
  }
  return count;
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr uint32_t k_num_sizes = num_class1_small_sizes(k_chunk_size) + num_class2_small_sizes(k_chunk_size) + num_large_sizes(k_chunk_size, k_max_size);


template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>> init_size_table() noexcept {
  std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>> table{0};

  for (uint32_t i = 0; i < num_class1_small_sizes(k_chunk_size); ++i) {
    if (k_chunk_size < k_class1_small_size_table[i]) break;
    table[i] = k_class1_small_size_table[i];
  }

  std::size_t offset = num_class1_small_sizes(k_chunk_size);
  std::size_t size = k_min_class2_small_size;
  for (uint32_t i = 0; i < num_class2_small_sizes(k_chunk_size); ++i) {
    table[i + offset] = size;
    size *= 2;
  }

  offset += num_class2_small_sizes(k_chunk_size);
  size = k_chunk_size;
  for (uint32_t i = 0; i < num_large_sizes(k_chunk_size, k_max_size); ++i) {
    table[i + offset] = size;
    size *= 2;
  }

  return table;
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr std::array<std::size_t, k_num_sizes<k_chunk_size, k_max_size>> k_size_table = init_size_table<k_chunk_size, k_max_size>();

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr int32_t find_in_size_table(const std::size_t size, const uint32_t offset = 0) noexcept {
  for (uint32_t i = offset; i < k_size_table<k_chunk_size, k_max_size>.size(); ++i) {
    if (size <= k_size_table<k_chunk_size, k_max_size>[i]) return static_cast<int32_t>(i);
  }
  return -1; // Error
}

template <std::size_t k_chunk_size, std::size_t k_max_size>
constexpr int32_t object_size_index(const std::size_t size) noexcept {

  if (size <= k_size_table<k_chunk_size, k_max_size>[0]) return 0;

  if (size <= 320) {
    const int z = __builtin_clzll(size);
    const std::size_t r = size + (1ul << (61 - z)) - 1;
    const int y = __builtin_clzll(r);
    const int index = static_cast<int>(4 * (60 - y) + ((r >> (61 - y)) & 3));
    return static_cast<uint32_t>(index);
  }

  const int pos_320 = find_in_size_table<k_chunk_size, k_max_size>(320);
  if constexpr (pos_320 < 0) return -1; // Error;

  return find_in_size_table<k_chunk_size, k_max_size>(size, static_cast<uint32_t>(pos_320) + 1);
}

} // namespace object_size_manager_detail

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
    return dtl::num_class1_small_sizes(k_chunk_size) + dtl::num_class2_small_sizes(k_chunk_size);
  }

  static constexpr size_type num_large_sizes() noexcept {
    return dtl::num_large_sizes(k_chunk_size, k_max_object_size);
  }

  static constexpr int32_t index(const size_type size) noexcept {
    return dtl::object_size_index<k_chunk_size, k_max_object_size>(size);
  }
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_KERNEL_OBJECT_SIZE_MANAGER_HPP
