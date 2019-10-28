// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <cmath>
#include <vector>
#include <random>
#include <memory>
#include <cstddef>

#include <metall/detail/utility/bitset.hpp>
#include <metall/v0/kernel/multilayer_bitset.hpp>

namespace {
using metall::v0::kernel::multilayer_bitset_detail::index_depth;
using metall::v0::kernel::multilayer_bitset_detail::num_internal_trees;

TEST(MultilayerBitsetTest, NumLayers) {
  for (uint64_t num_local_blocks = 1; num_local_blocks <= 4; num_local_blocks *= 2) {
    for (uint64_t num_blocks = 0; num_blocks <= 4 * 4 * 4 * 4; ++num_blocks) {
      const uint64_t depth = index_depth(num_blocks, num_local_blocks);
      if (num_local_blocks == 1) {
        ASSERT_EQ(depth, num_blocks); // The number of blocks must be equal to the depth
      } else if (num_blocks == 0) {
        ASSERT_EQ(depth, 0);  // No layer is needed
      } else if (num_blocks == 1) {
        ASSERT_EQ(depth, 1); // Just one layer is enough
      } else if (num_blocks < num_local_blocks) {
        ASSERT_EQ(depth, 1); // Just one layer is enough
      } else {
        ASSERT_LE(1, depth); // Must be at least one depth
        ASSERT_LT(std::pow(num_local_blocks, depth - 1), num_blocks); // The one less depth must not be enough
        ASSERT_LE(num_blocks, std::pow(num_local_blocks, depth)); // Must be enough blocks
      }
    }
  }
}

TEST(MultilayerBitsetTest, NumInternalTrees) {
  for (uint64_t num_local_blocks = 1; num_local_blocks <= 4; num_local_blocks *= 2) {
    for (uint64_t num_blocks = 0; num_blocks <= 4 * 4 * 4 * 4; ++num_blocks) {
      const uint64_t depth = index_depth(num_blocks, num_local_blocks);
      const uint64_t num_trees = num_internal_trees(num_blocks, num_local_blocks, depth);
      if (num_blocks == 0 || num_blocks <= num_local_blocks) {
        ASSERT_EQ(num_trees, 0);
      } else if (num_local_blocks == 1) {
        ASSERT_EQ(num_trees, 1);
      } else {
        ASSERT_GT(num_trees, 1); // Must be more than 1
        // Must be enough capacity
        ASSERT_LE(num_blocks, num_trees * std::pow(num_local_blocks, depth - 1));
        // Must not be enough capacity
        ASSERT_GT(num_blocks, (num_trees - 1) * std::pow(num_local_blocks, depth - 1));
      }
    }
  }
}

TEST(MultilayerBitsetTest, FindAndSet) {
  for (uint64_t num_bits = 1; num_bits < (64ULL * 64 * 64 * 64); num_bits *= 2) { // Test up to 4 layers
    metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>> bitset;
    auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>>::rebind_allocator_type();
    bitset.allocate(num_bits, allocator);
    for (uint64_t i = 0; i < num_bits; ++i) {
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
      ASSERT_TRUE(bitset.get(num_bits, i));
    }
    bitset.free(num_bits, allocator);
  }
}

TEST(MultilayerBitsetTest, Reset) {
  for (uint64_t num_bits = 1; num_bits < (64ULL * 64 * 64 * 64); num_bits *= 2) { // Test up to 4 layers
    metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>> bitset;
    auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>>::rebind_allocator_type();
    bitset.allocate(num_bits, allocator);
    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.find_and_set(num_bits);
    }

    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.reset(num_bits, i);
      ASSERT_FALSE(bitset.get(num_bits, i));
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
    }

    bitset.free(num_bits, allocator);
  }
}

void RandomSetHelper(const std::size_t num_bits) {
  SCOPED_TRACE("num_bits = " + std::to_string(num_bits));

  metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>> bitset;
  auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>>::rebind_allocator_type();
  bitset.allocate(num_bits, allocator);

  std::vector<bool> reference(num_bits, false);

  std::mt19937_64 mt;
  std::uniform_int_distribution<uint64_t> rand_dist(0, num_bits * 2 - 2);

  uint64_t smallest_free_pos = 0;
  for (uint64_t i = 0; i < num_bits * 2ULL; ++i) { // Just repeat many times
    const auto random_value = rand_dist(mt);
    const bool do_set = random_value / num_bits;

    if (do_set && !reference[smallest_free_pos]) { // Set
      ASSERT_EQ(bitset.find_and_set(num_bits), smallest_free_pos);
      reference[smallest_free_pos] = true;

      // Make sure that min_free_pos is the smallest available spot
      for (uint64_t p = 0; p <= smallest_free_pos; ++p) {
        ASSERT_TRUE(reference[p]);
      }

      // Find the next available spot
      for (uint64_t p = smallest_free_pos + 1; p < reference.size(); ++p) {
        if (!reference[p]) {
          smallest_free_pos = p;
          break;
        }
      }

    } else if (!do_set) { // Reset
      const uint64_t pos = random_value % num_bits;
      if (!reference[pos]) continue;
      bitset.reset(num_bits, pos);
      reference[pos] = false;
      smallest_free_pos = std::min(smallest_free_pos, pos);
    }
  }

  bitset.free(num_bits, allocator);
}

TEST(MultilayerBitsetTest, RandomSet) {

  // 1 layer
  RandomSetHelper(1ULL << 0);
  RandomSetHelper(1ULL << 1);
  RandomSetHelper(1ULL << 2);
  RandomSetHelper(1ULL << 3);
  RandomSetHelper(1ULL << 4);
  RandomSetHelper(1ULL << 5);
  RandomSetHelper(1ULL << 6);

  // 2 layers
  RandomSetHelper(1ULL << 7);
  RandomSetHelper(1ULL << 10);
  RandomSetHelper(1ULL << 12);

// --- perform tests more than 2 layers only when it is needed -- //

  // 3 layers
  //RandomSetHelper(1ULL << 13);
  //RandomSetHelper(1ULL << 16);
  //RandomSetHelper(1ULL << 18); // 2^21 / 2^3 = 2^18

  // 4 layers
  //RandomSetHelper(1ULL << 19);
  //RandomSetHelper(1ULL << 22);
  //RandomSetHelper(1ULL << 24);
}

void RandomSetAndResetHelper(const std::size_t num_bits) {
  SCOPED_TRACE("num_bits = " + std::to_string(num_bits));

  metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>> bitset;
  auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<std::byte>>::rebind_allocator_type();
  bitset.allocate(num_bits, allocator);

  std::vector<bool> reference(num_bits, false);

  std::mt19937_64 mt;
  std::uniform_int_distribution<uint64_t> rand_dist(0, num_bits - 1);

  for (uint64_t i = 0; i < num_bits * 2ULL; ++i) { // Just repeat many times
    const auto position = rand_dist(mt);

    ASSERT_EQ(bitset.get(num_bits, position), reference[position]);

    if (bitset.get(num_bits, position)) { // If the current value is true, then reset the value
      bitset.reset(num_bits, position);
      reference[position] = false;
    } else {
      const auto set_position = bitset.find_and_set(num_bits);
      reference[set_position] = true;
    }

    // Make sure all bits match to the reference
    for (uint64_t pos = 0; pos < num_bits; ++pos) {
      ASSERT_EQ(bitset.get(num_bits, pos), reference[pos]);
    }
  }

  bitset.free(num_bits, allocator);
}

TEST(MultilayerBitsetTest, RandomSetAndReset) {

  // 1 layer
  RandomSetAndResetHelper(1ULL << 0);
  RandomSetAndResetHelper(1ULL << 1);
  RandomSetAndResetHelper(1ULL << 2);
  RandomSetAndResetHelper(1ULL << 3);
  RandomSetAndResetHelper(1ULL << 4);
  RandomSetAndResetHelper(1ULL << 5);
  RandomSetAndResetHelper(1ULL << 6);

  // 2 layers
  RandomSetAndResetHelper(1ULL << 7);
  RandomSetAndResetHelper(1ULL << 10);
  RandomSetAndResetHelper(1ULL << 12);

  // --- perform tests more than 2 layers only when it is needed -- //

  // 3 layers
  //RandomSetAndResetHelper(1ULL << 13);
  //RandomSetAndResetHelper(1ULL << 16);
  //RandomSetAndResetHelper(1ULL << 18); // 2^21 / 2^3 = 2^18

  // 4 layers
  //RandomSetAndResetHelper(1ULL << 19);
  //RandomSetAndResetHelper(1ULL << 22);
  //RandomSetAndResetHelper(1ULL << 24);
}
}
