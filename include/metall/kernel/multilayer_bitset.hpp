// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
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
#include <array>
#include <utility>

#include <metall/detail/utilities.hpp>
#include <metall/detail/bitset.hpp>
#include <metall/detail/builtin_functions.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

namespace multilayer_bitset_detail {

/// \brief The i-th element is the number of required layers to manage i^2 bits.
/// \warning This array supports up to 2^24 bits for now.
/// Assumes that each block can hold 64 bits.
static constexpr std::array<std::size_t, 25> k_num_layers_table = {
    1, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4};

/// \brief The i-th element is the number of required 'index blocks' to manage
/// i^2 bits. A bit in an index block represents whether all bits of the
/// corresponding child blocks are true or not. \warning This array supports up
/// to 2^24 bits for now. Assumes that each block can hold 64 bits.
static constexpr std::array<std::size_t, 25> k_num_index_blocks_table = {
    0, 0, 0, 0,  0,  0,  0,   1,   1,   1,    1,    1,   1,
    3, 5, 9, 17, 33, 65, 131, 261, 521, 1041, 2081, 4161};

/// \brief k_num_blocks_table[i][k] is the number of required blocks to manage
/// i^2 bits at k-th layer. \warning This array supports up to 2^24 bits for
/// now. Assumes that each block can hold 64 bits.
static constexpr std::array<std::array<std::size_t, 4>, 25> k_num_blocks_table =
    {{{{1, 0, 0, 0}},         {{1, 0, 0, 0}},         {{1, 0, 0, 0}},
      {{1, 0, 0, 0}},         {{1, 0, 0, 0}},         {{1, 0, 0, 0}},
      {{1, 0, 0, 0}},         {{1, 2, 0, 0}},         {{1, 4, 0, 0}},
      {{1, 8, 0, 0}},         {{1, 16, 0, 0}},        {{1, 32, 0, 0}},
      {{1, 64, 0, 0}},        {{1, 2, 128, 0}},       {{1, 4, 256, 0}},
      {{1, 8, 512, 0}},       {{1, 16, 1024, 0}},     {{1, 32, 2048, 0}},
      {{1, 64, 4096, 0}},     {{1, 2, 128, 8192}},    {{1, 4, 256, 16384}},
      {{1, 8, 512, 32768}},   {{1, 16, 1024, 65536}}, {{1, 32, 2048, 131072}},
      {{1, 64, 4096, 262144}}}};
}  // namespace multilayer_bitset_detail

namespace {
namespace bs = metall::mtlldetail::bitset_detail;
namespace mlbs = multilayer_bitset_detail;
}  // namespace

/// \brief A bitset class that uses multiple layers of bitsets efficiently
/// manage bits. To remove a space for storing the number of bits to hold, this
/// class asks users to manage such information on their sides. Thus, users must
/// not ask more bits when the bitset is full.
class multilayer_bitset {
 private:
  union block_holder {
   public:
    using block_type = uint64_t;
    static_assert(sizeof(void *) == sizeof(block_type),
                  "This code works only on 64 bits systems");

    block_holder() = default;
    ~block_holder() = default;
    block_holder(const block_holder &) = default;
    block_holder(block_holder &&other) noexcept = default;
    block_holder &operator=(const block_holder &) = default;
    block_holder &operator=(block_holder &&other) noexcept = default;

    void reset() {
      bs::erase(&block);
      array = nullptr;
    }

    // Construct a bitset into this space directly if #of required bits are
    // small.
    block_type block;
    // Holds a pointer to a multi-layer bitset table.
    block_type *array;
  };

  using block_type = typename block_holder::block_type;
  static constexpr std::size_t k_num_bits_in_block = sizeof(block_type) * 8;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using bit_position_type = std::size_t;

  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  multilayer_bitset() = default;
  ~multilayer_bitset() = default;
  multilayer_bitset(const multilayer_bitset &) = default;
  multilayer_bitset(multilayer_bitset &&other) noexcept = default;
  multilayer_bitset &operator=(const multilayer_bitset &) = default;
  multilayer_bitset &operator=(multilayer_bitset &&other) noexcept = default;

  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief Returns the maximum number of bits in a block.
  /// \return  The maximum number of bits in a block.
  static constexpr std::size_t block_size() noexcept {
    static_assert(
        mdtl::next_power_of_2(k_num_bits_in_block) == k_num_bits_in_block,
        "k_num_bits_in_block must be a power of 2");
    return k_num_bits_in_block;
  }

  /// \brief Returns the maximum number of bits this class can handle.
  /// \return The maximum number of bits this class can handle.
  static constexpr std::size_t max_size() noexcept {
    return std::numeric_limits<bit_position_type>::max() / 2;
  }

  /// \brief Resets internal variables.
  /// This function does not allocate or free memory.
  void reset() { m_data.reset(); }

  /// \brief Allocates internal space.
  /// \param size The number of bits this bitset holds.
  bool allocate(const std::size_t size) {
    assert(mdtl::next_power_of_2(size) < max_size());
    if (size <= block_size()) {
      bs::erase(&m_data.block);  // manage the bits without allocating layers.
      return true;
    } else {
      return allocate_multilayer_bitset(size);
    }
    assert(false);
    return false;
  }

  /// \brief Users have to explicitly free bitset table
  /// \param size The number of bits this bitset holds.
  void free(const std::size_t size) {
    if (block_size() < size) {
      free_multilayer_bitset();
    }
  }

  /// \brief Finds a bit whose value is false and sets the bit to true.
  /// \param size The number of bits to this bitset holds.
  /// \return The position of the found bit
  bit_position_type find_and_set(const std::size_t size) {
    if (size <= block_size())
      return find_and_set_in_single_block();
    else
      return find_and_set_in_multilayers(size);
  }

  /// \brief Find multiple number of false bits and set them to true.
  /// \param size The number of bits this bitset holds.
  /// \param num_bits_to_find The number of bits to find.
  /// \param bit_positions A buffer to store found bits.
  /// \return The number of found bits.
  /// \warning Users must make sure that there are enough number of available
  /// bits when calling this function.
  void find_and_set_many(const std::size_t size,
                         const std::size_t num_bits_to_find,
                         bit_position_type *const bit_positions) {
    if (num_bits_to_find == 0 || !bit_positions) return;
    if (size <= block_size())
      find_and_set_many_in_single_block(num_bits_to_find, bit_positions);
    else
      find_and_set_many_in_multilayers(size, num_bits_to_find, bit_positions);
  }

  /// \brief Sets a bit to false.
  /// \param size The number of bits this bitset holds.
  /// \param bit_position The position of the bit to reset (set false).
  void reset(const std::size_t size, const bit_position_type bit_position) {
    if (size <= block_size()) {
      bs::reset(&m_data.block, bit_position);
    } else {
      reset_bit_in_multilayers(size, bit_position);
    }
  }

  /// \brief Gets the value of a bit.
  /// \param size The number of bits this bitset holds.
  /// \param bit_position The position of the bit to find (set false).
  /// \return Boolean value of the bit.
  bool get(const std::size_t size, const bit_position_type bit_position) const {
    if (size <= block_size()) {
      return bs::get(&m_data.block, bit_position);
    } else {
      return get_in_multilayers(size, bit_position);
    }
  }

  /// \brief Serializes the internal data.
  /// \param size The number of bits this bitset holds.
  /// \return Serialized data as std::string.
  std::string serialize(const std::size_t size) const {
    if (size <= block_size()) {
      return std::to_string(static_cast<uint64_t>(m_data.block));
    } else {
      std::string buf;
      const std::size_t nb = num_all_blocks(size);
      for (std::size_t i = 0; i < nb; ++i) {
        if (i != 0) buf += " ";
        buf += std::to_string(static_cast<uint64_t>(m_data.array[i]));
      }
      return buf;
    }
  }

  /// \brief Deserializes data.
  /// \param size The number of bits this bitset holds.
  /// \param input_string A string to deserialize.
  /// \return Returns true on success; otherwise, false.
  bool deserialize(const std::size_t size, const std::string &input_string) {
    if (size <= block_size()) {
      std::istringstream ss(input_string);
      uint64_t buf;
      ss >> buf;
      m_data.block = buf;
    } else {
      const std::size_t num_blocks = num_all_blocks(size);
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
  // -------------------- //
  // Private methods
  // -------------------- //
  // ---------- Allocation and free ---------- //
  bool allocate_multilayer_bitset(const std::size_t size) {
    const std::size_t num_blocks = num_all_blocks(size);
    m_data.array =
        static_cast<block_type *>(std::malloc(num_blocks * sizeof(block_type)));
    if (!m_data.array) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Cannot allocate multi-layer bitset");
      return false;
    }
    std::fill(&m_data.array[0], &m_data.array[num_blocks], 0);

    return true;
  }

  void free_multilayer_bitset() {
    std::free(m_data.array);
    m_data.reset();
  }

  // ---------- Find, set, and reset bits ---------- //
  bit_position_type find_and_set_in_single_block() {
    assert(!bs::full_block(m_data.block));
    const auto pos = find_first_false_bit_in_block(m_data.block);
    assert(pos < block_size());
    bs::set(&m_data.block, pos);
    return pos;
  }

  void find_and_set_many_in_single_block(
      const std::size_t num_bits_to_find,
      bit_position_type *const bit_positions) {
    assert(!bs::full_block(m_data.block));
    for (std::size_t i = 0; i < num_bits_to_find; ++i) {
      bit_positions[i] = find_first_false_bit_in_block(m_data.block);
      assert(bit_positions[i] < block_size());
      bs::set(&m_data.block, bit_positions[i]);
    }
  }

  bit_position_type find_and_set_in_multilayers(const std::size_t size) {
    const std::size_t idx = mdtl::log2_dynamic(mdtl::next_power_of_2(size));

    assert(idx < mlbs::k_num_layers_table.size());
    assert(idx < mlbs::k_num_blocks_table.size());
    const bit_position_type bit_pos_in_leaf_layer = find_in_multilayers(
        mlbs::k_num_layers_table[idx], mlbs::k_num_blocks_table[idx]);
    assert(bit_pos_in_leaf_layer < size);

    assert(idx < mlbs::k_num_index_blocks_table.size());
    set_in_multilayers(mlbs::k_num_layers_table[idx],
                       mlbs::k_num_index_blocks_table[idx],
                       mlbs::k_num_blocks_table[idx], bit_pos_in_leaf_layer);
    return bit_pos_in_leaf_layer;
  }

  /// \brief Finds a consecutive false bits and set them to full.
  /// Return The first bit position of the found chunk and the number of bits.
  void find_and_set_many_in_multilayers(
      const std::size_t size, const std::size_t num_requested_bits,
      bit_position_type *const found_bit_positions) {
    if (size == 0 || num_requested_bits == 0 || !found_bit_positions) return;
    const std::size_t idx = mdtl::log2_dynamic(mdtl::next_power_of_2(size));
    assert(idx < mlbs::k_num_layers_table.size());
    assert(idx < mlbs::k_num_blocks_table.size());
    assert(idx < mlbs::k_num_index_blocks_table.size());

    auto bit_pos_in_leaf = find_in_multilayers(mlbs::k_num_layers_table[idx],
                                               mlbs::k_num_blocks_table[idx]);
    assert(bit_pos_in_leaf < size);

    std::size_t count = 0;
    while (true) {
      const auto block_pos_in_leaf = bit_pos_in_leaf / block_size();
      const auto global_block_pos =
          block_pos_in_leaf + mlbs::k_num_index_blocks_table[idx];

      if (bs::empty_block(m_data.array[global_block_pos])) {
        const auto n = std::min(num_requested_bits - count, block_size());
        for (std::size_t i = 0; i < n; ++i, ++count, ++bit_pos_in_leaf) {
          found_bit_positions[count] = bit_pos_in_leaf;
        }
        m_data.array[global_block_pos] |= bs::generate_mask<block_size()>(0, n);
        ;
        set_in_multilayers(mlbs::k_num_layers_table[idx],
                           mlbs::k_num_index_blocks_table[idx],
                           mlbs::k_num_blocks_table[idx], bit_pos_in_leaf - 1);
        if (count == num_requested_bits) return;
      } else {
        for (auto i = bit_pos_in_leaf % block_size(); i < block_size();
             ++i, ++bit_pos_in_leaf) {
          assert(bit_pos_in_leaf < size);
          if (bs::get(&m_data.array[global_block_pos], i)) continue;

          found_bit_positions[count] = bit_pos_in_leaf;
          ++count;
          set_in_multilayers(mlbs::k_num_layers_table[idx],
                             mlbs::k_num_index_blocks_table[idx],
                             mlbs::k_num_blocks_table[idx], bit_pos_in_leaf);
          if (count == num_requested_bits) return;
        }
      }

      if (bs::full_block(
              m_data.array[global_block_pos + 1])) {  // Is next block is full?
        bit_pos_in_leaf = find_in_multilayers(mlbs::k_num_layers_table[idx],
                                              mlbs::k_num_blocks_table[idx]);
      } else {
        // Find next available bit in the next block
        const auto bit_pos_in_block =
            find_first_false_bit_in_block(m_data.array[global_block_pos + 1]);
        bit_pos_in_leaf =
            (block_pos_in_leaf + 1) * block_size() + bit_pos_in_block;
        assert(bit_pos_in_leaf < size);
      }
    }
  }

  template <std::size_t N>
  bit_position_type find_in_multilayers(
      const std::size_t num_layers,
      const std::array<std::size_t, N> &num_blocks) const {
    bit_position_type bit_pos =
        0;  // bit position in the layer, block position in the next layer.
    std::size_t num_parent_blocks = 0;
    for (int layer = 0; layer < static_cast<int>(num_layers); ++layer) {
      num_parent_blocks += (layer == 0) ? 0 : num_blocks[layer - 1];
      const bit_position_type block_pos = num_parent_blocks + bit_pos;

      assert(!bs::full_block(m_data.array[block_pos]));
      const auto first_zero_pos_in_block =
          find_first_false_bit_in_block(m_data.array[block_pos]);
      assert(first_zero_pos_in_block < block_size());

      bit_pos = first_zero_pos_in_block +
                block_size() * (block_pos - num_parent_blocks);
    }

    return bit_pos;  // found bit position in leaf
  }

  /// \brief Set a bit to true and propagate the information to index blocks.
  /// This function does not care existing values on purpose as the feature is
  /// useful.
  template <std::size_t N>
  void set_in_multilayers(const std::size_t num_layers,
                          std::size_t num_index_blocks,
                          const std::array<std::size_t, N> &num_blocks_table,
                          const bit_position_type bit_pos_in_leaf) {
    std::size_t num_parent_blocks = num_index_blocks;
    bit_position_type bit_pos = bit_pos_in_leaf;

    for (int layer = static_cast<int>(num_layers) - 1; layer >= 0; --layer) {
      const bit_position_type block_pos =
          num_parent_blocks + bit_pos / block_size();
      bs::set(&m_data.array[block_pos],
              bs::local_index<block_type>(static_cast<std::size_t>(bit_pos)));

      if (!bs::full_block(m_data.array[block_pos])) break;
      if (layer == 0) break;

      num_parent_blocks -= num_blocks_table[layer - 1];
      bit_pos = bit_pos / block_size();
    }
  }

  void reset_bit_in_multilayers(const std::size_t size,
                                const bit_position_type bit_pos) {
    const std::size_t idx = mdtl::log2_dynamic(mdtl::next_power_of_2(size));

    assert(idx < mlbs::k_num_layers_table.size());
    const std::size_t num_layers = mlbs::k_num_layers_table[idx];
    assert(idx < mlbs::k_num_index_blocks_table.size());
    std::size_t num_parent_index_block = mlbs::k_num_index_blocks_table[idx];
    bit_position_type bit_pos_in_current_layer = bit_pos;

    for (int layer = static_cast<int>(num_layers) - 1; layer >= 0; --layer) {
      const auto block_pos =
          num_parent_index_block + bit_pos_in_current_layer / block_size();
      const bool was_full = bs::full_block(m_data.array[block_pos]);
      bs::reset(&m_data.array[num_parent_index_block],
                static_cast<std::size_t>(bit_pos_in_current_layer));
      if (!was_full || layer == 0) break;
      assert(idx < mlbs::k_num_blocks_table.size());
      assert(static_cast<std::size_t>(layer - 1) <
             mlbs::k_num_blocks_table.front().size());
      num_parent_index_block -= mlbs::k_num_blocks_table[idx][layer - 1];
      bit_pos_in_current_layer = bit_pos_in_current_layer / block_size();
    }
  }

  bool get_in_multilayers(const std::size_t size,
                          const bit_position_type bit_pos) const {
    const std::size_t idx = mdtl::log2_dynamic(mdtl::next_power_of_2(size));
    assert(idx < mlbs::k_num_index_blocks_table.size());
    const std::size_t num_parent_index_block =
        mlbs::k_num_index_blocks_table[idx];
    return bs::get(&m_data.array[num_parent_index_block],
                   static_cast<std::size_t>(bit_pos));
  }

  // ---------- Utilities ---------- //
  /// \brief Returns the index of the first false bit in the block from the most
  /// significant bit position. \warning If the block is full, the result is
  /// undefined.
  static bit_position_type find_first_false_bit_in_block(
      block_type block) noexcept {
    static_assert(sizeof(block_type) == sizeof(uint64_t),
                  "This implementation works on only 64bit system now");
    static_assert(sizeof(unsigned long long) == sizeof(uint64_t),
                  "sizeof(unsigned long long) != sizeof(uint64_t)");

    return bs::empty_block(block) ? 0 : mdtl::clzll(~block);
  }

  std::size_t num_all_blocks(const std::size_t size) const {
    const std::size_t idx = mdtl::log2_dynamic(mdtl::next_power_of_2(size));
    std::size_t num_blocks = 0;
    assert(idx < mlbs::k_num_layers_table.size());
    for (int layer = 0; layer < static_cast<int>(mlbs::k_num_layers_table[idx]);
         ++layer) {
      assert(idx < mlbs::k_num_blocks_table.size());
      assert(static_cast<std::size_t>(layer) <
             mlbs::k_num_blocks_table.front().size());
      num_blocks += mlbs::k_num_blocks_table[idx][layer];
    }
    return num_blocks;
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  block_holder m_data;
};

}  // namespace kernel
}  // namespace metall
#endif  // METALL_MULTILAYER_BITSET_HPP
