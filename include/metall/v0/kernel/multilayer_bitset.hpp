// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_MULTILAYER_BITSET_HPP
#define METALL_MULTILAYER_BITSET_HPP

#include <iostream>
#include <type_traits>
#include <cstdint>
#include <limits>
#include <cassert>
#include <tuple>
#include <string>
#include <sstream>
#include <memory>
#include <algorithm>

#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/bitset.hpp>
#include <metall/detail/utility/builtin_functions.hpp>

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

namespace multilayer_bitset_detail {

inline constexpr uint64_t index_depth(const uint64_t num_blocks, const uint64_t num_local_blocks) noexcept {
  return (num_blocks == 0) ? 0 : (num_local_blocks == 1) ? num_blocks : util::log_cpt(num_blocks - 1, num_local_blocks)
      + 1;
}

inline constexpr uint64_t num_internal_trees(const uint64_t num_blocks,
                                      const uint64_t num_local_blocks,
                                      const uint64_t index_depth) noexcept {
  return (num_blocks == 0 || index_depth <= 1) ? 0
                                               : ((num_local_blocks <= 2) ? num_local_blocks
                                                                          : ((num_blocks - 1)
              / (util::power_cpt(num_local_blocks, index_depth - 1)) + 1));
}

inline constexpr uint64_t num_index_blocks(const uint64_t num_local_blocks,
                                    const uint64_t index_depth,
                                    const uint64_t num_full_trees) noexcept {
  return (index_depth <= 1) ? index_depth // no top layer or only top layer
                            : 1 + num_full_trees
          * util::power_cpt(num_local_blocks, index_depth - 2); // top layer and #blocks for the trees
}


// Index is log2(#of bits)
// !!! This array supports up to 4 layers for now !!!
static constexpr std::size_t k_num_layers_table[25]
    = {1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4};
static constexpr std::size_t k_num_index_blocks_table[25]
    = {0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 3, 5, 9, 17, 33, 65, 131, 261, 521, 1041, 2081, 4161};
static constexpr std::size_t k_num_blocks_table[25][4] = {
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 0, 0, 0},
    {1, 2, 0, 0},
    {1, 4, 0, 0},
    {1, 8, 0, 0},
    {1, 16, 0, 0},
    {1, 32, 0, 0},
    {1, 64, 0, 0},
    {1, 2, 128, 0},
    {1, 4, 256, 0},
    {1, 8, 512, 0},
    {1, 16, 1024, 0},
    {1, 32, 2048, 0},
    {1, 64, 4096, 0},
    {1, 2, 128, 8192},
    {1, 4, 256, 16384},
    {1, 8, 512, 32768},
    {1, 16, 1024, 65536},
    {1, 32, 2048, 131072},
    {1, 64, 4096, 262144}
};
} // namespace multilayer_bitset_detail

namespace {
namespace bs = metall::detail::utility::bitset_detail;
namespace mlbs = multilayer_bitset_detail;
}

template <typename allocator_type>
class multilayer_bitset {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  union block_holder {
   public:
    using block_type // Assumes <= 64 bits environment
    = typename std::conditional<sizeof(void *) == sizeof(uint8_t),
                                uint8_t,
                                typename std::conditional<sizeof(void *) == sizeof(uint16_t), uint16_t,
                                                          typename std::conditional<sizeof(void *) == sizeof(uint32_t),
                                                                                    uint32_t,
                                                                                    uint64_t>::type>::type>::type;

    block_holder() = default;
    ~block_holder() = default;
    block_holder(const block_holder &) = default;
    block_holder(block_holder &&other) = default;
    block_holder &operator=(const block_holder &) = default;
    block_holder &operator=(block_holder &&other) = default;

    block_type block; // Construct a bitset into this space directly if #of required bits are small
    block_type *array; // Holds a pointer to a multi-layer bitset table
  };
  using block_type = typename block_holder::block_type;
  static constexpr std::size_t k_num_bits_in_block = sizeof(block_type) * 8;

  using rebind_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<block_type>;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  multilayer_bitset() = default;
  ~multilayer_bitset() = default;
  multilayer_bitset(const multilayer_bitset &) = default;
  multilayer_bitset(multilayer_bitset &&other) = default;
  multilayer_bitset &operator=(const multilayer_bitset &) = default;
  multilayer_bitset &operator=(multilayer_bitset &&other) = default;

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  /// \brief Allocates internal space
  void allocate(const std::size_t num_bits, rebind_allocator_type& allocator) {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      m_data.block = 0;
    } else {
      allocate_multilayer_bitset(num_bits_power2, allocator);
    }
  }

  /// \brief Users have to explicitly free bitset table
  void free(const std::size_t num_bits, rebind_allocator_type& allocator) {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (k_num_bits_in_block < num_bits_power2) {
      free_multilayer_bitset(num_bits_power2, allocator);
    }
  }

  /// \brief Finds an negative bit and sets it to positive
  /// \return The position of the found bit
  ssize_t find_and_set(const std::size_t num_bits) {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      return find_and_set_in_single_block(num_bits_power2);
    } else {
      return find_and_set_in_multilayers(num_bits_power2);
    }
  }

  /// \brief Resets the given bit
  void reset(const std::size_t num_bits, const ssize_t bit_no) {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      bs::reset(&m_data.block, bit_no);
    } else {
      reset_bit_in_multilayers(num_bits_power2, bit_no);
    }
  }

  /// \brief Gets the value of a bit
  /// \return Boolean value of the bit
  bool get(const std::size_t num_bits, const ssize_t bit_no) const {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      return bs::get(&m_data.block, bit_no);
    } else {
      return get_in_multilayers(num_bits_power2, bit_no);
    }
  }

  block_holder &data() {
    return m_data;
  }

  std::string serialize(const std::size_t num_bits) const {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      return std::to_string(static_cast<uint64_t>(m_data.block));
    } else {
      std::string buf;
      const std::size_t nb = num_all_blokcs(num_bits_power2);
      for (std::size_t i = 0; i < nb; ++i) {
        if (i != 0) buf += " ";
        buf += std::to_string(static_cast<uint64_t>(m_data.array[i]));
      }
      return buf;
    }
  }

  bool deserialize(const std::size_t num_bits, const std::string &input_string) {
    const std::size_t num_bits_power2 = util::next_power_of_2(num_bits);
    if (num_bits_power2 <= k_num_bits_in_block) {
      std::istringstream ss(input_string);
      uint64_t buf;
      ss >> buf;
      m_data.block = buf;
    } else {
      const std::size_t num_blocks = num_all_blokcs(num_bits_power2);
      std::stringstream ss(input_string);

      std::size_t count = 0;
      uint64_t buf;
      while (ss >> buf) {
        if (num_blocks < count) {
          return false;
        }
        m_data.array[count] = static_cast<block_type>(buf);
        ++count;
      }
      if (count != num_blocks) return false;
    }
    return true;
  }

 private:
  /// -------------------------------------------------------------------------------- ///
  /// Private methods
  /// -------------------------------------------------------------------------------- ///
  // -------------------- Allocation and free -------------------- //
  void allocate_multilayer_bitset(const std::size_t num_bits_power2, rebind_allocator_type& allocator) {
    const std::size_t num_blocks = num_all_blokcs(num_bits_power2);
    m_data.array = std::allocator_traits<rebind_allocator_type>::allocate(allocator, num_blocks);
    std::fill(&m_data.array[0], &m_data.array[num_blocks], 0);

    if (!m_data.array) {
      std::cerr << "Cannot allocate multi-layer bitset" << std::endl;
      std::abort();
    }
  }

  void free_multilayer_bitset(const std::size_t num_bits_power2, rebind_allocator_type& allocator) {
    std::allocator_traits<rebind_allocator_type>::deallocate(allocator, m_data.array, num_all_blokcs(num_bits_power2));
  }

  // -------------------- Find, set, and reset bits -------------------- //
  ssize_t find_and_set_in_single_block(const std::size_t num_bits_power2) {
    if (full_block(m_data.block)) return -1;
    const auto index = find_first_zero_in_block(m_data.block);
    if (static_cast<std::size_t>(index) < num_bits_power2) {
      bs::set(&m_data.block, index);
      return index;
    }
    return -1;
  }

  ssize_t find_and_set_in_multilayers(const std::size_t num_bits_power2) {
    const std::size_t idx = util::log2_dynamic(num_bits_power2);

    const ssize_t bit_index_in_leaf_layer = find_in_multilayers(mlbs::k_num_layers_table[idx], mlbs::k_num_blocks_table[idx]);
    assert(bit_index_in_leaf_layer < static_cast<ssize_t>(num_bits_power2));
    if (bit_index_in_leaf_layer < 0) return -1; // Error

    set_in_multilayers(mlbs::k_num_layers_table[idx], mlbs::k_num_index_blocks_table[idx],
                       mlbs::k_num_blocks_table[idx], bit_index_in_leaf_layer);
    return bit_index_in_leaf_layer;
  }

  ssize_t find_in_multilayers(const std::size_t num_layers, const std::size_t *const num_blocks) const {
    if (full_block(m_data.array[0]))
      return -1; // Error

    ssize_t bit_index = 0;
    std::size_t num_parent_blocks = 0;
    for (int layer = 0; layer < static_cast<int>(num_layers); ++layer) {
      num_parent_blocks += (layer == 0) ? 0 : num_blocks[layer - 1];
      const ssize_t block_index = num_parent_blocks + bit_index;
      bit_index = find_first_zero_in_block(m_data.array[block_index])
          + k_num_bits_in_block * (block_index - num_parent_blocks);
    }

    return bit_index;
  }

  void set_in_multilayers(const std::size_t num_layers, std::size_t num_index_blocks,
                          const std::size_t *const num_blocks_table, const ssize_t bit_index_in_leaf) {
    std::size_t num_parent_blocks = num_index_blocks;
    ssize_t bit_index = bit_index_in_leaf;

    for (int layer = static_cast<int>(num_layers) - 1; layer >= 0; --layer) {
      const ssize_t block_index = num_parent_blocks + bit_index / k_num_bits_in_block;
      bs::set(&m_data.array[block_index], bs::local_index<block_type>(static_cast<std::size_t>(bit_index)));

      if (!full_block(m_data.array[block_index])) break;
      if (layer == 0) break;

      num_parent_blocks -= num_blocks_table[layer - 1];
      bit_index = bit_index / k_num_bits_in_block;
    }
  }

  void reset_bit_in_multilayers(const std::size_t num_bits_power2, const ssize_t bit_index) {
    const std::size_t idx = util::log2_dynamic(num_bits_power2);

    const std::size_t num_layers = mlbs::k_num_layers_table[idx];
    std::size_t num_parent_index_block = mlbs::k_num_index_blocks_table[idx];
    ssize_t bit_index_in_current_layer = bit_index;

    for (ssize_t layer = num_layers - 1; layer >= 0; --layer) {
      const bool was_full = full_block(m_data.array[num_parent_index_block
          + bit_index_in_current_layer / k_num_bits_in_block]);
      bs::reset(&m_data.array[num_parent_index_block], static_cast<std::size_t>(bit_index_in_current_layer));
      if (!was_full || layer == 0) break;
      num_parent_index_block -= mlbs::k_num_blocks_table[idx][layer - 1];
      bit_index_in_current_layer = bit_index_in_current_layer / k_num_bits_in_block;
    }
  }

  bool get_in_multilayers(const std::size_t num_bits_power2, const ssize_t bit_index) const {
    const std::size_t idx = util::log2_dynamic(num_bits_power2);
    const std::size_t num_parent_index_block = mlbs::k_num_index_blocks_table[idx];
    return bs::get(&m_data.array[num_parent_index_block], static_cast<std::size_t>(bit_index));
  }

  // -------------------- Utilities -------------------- //
  ssize_t find_first_zero_in_block(block_type block) const {
    static_assert(sizeof(block_type) == sizeof(uint64_t),
                  "This implementation works on only 64bit system now");
    static_assert(sizeof(unsigned long long) == sizeof(uint64_t), "sizeof(unsigned long long) != sizeof(uint64_t)");

    return (block == 0) ? 0 : util::clzll(~block);
  }

  bool full_block(block_type block) const {
    return block == ~static_cast<block_type>(0);
  }

  std::size_t num_all_blokcs(const std::size_t num_bits_power2) const {
    const std::size_t index = util::log2_dynamic(num_bits_power2);
    std::size_t num_blocks = 0;
    for (int layer = 0; layer < static_cast<int>(mlbs::k_num_layers_table[index]); ++layer) {
      num_blocks += mlbs::k_num_blocks_table[index][layer];
    }
    return num_blocks;
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  block_holder m_data;
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_MULTILAYER_BITSET_HPP
