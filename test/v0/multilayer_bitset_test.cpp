// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <cmath>
#include <vector>
#include <random>
#include <memory>

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
    metall::v0::kernel::multilayer_bitset<std::allocator<void>> bitset;
    auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<void>>::rebind_allocator_type();
    bitset.allocate(num_bits, allocator);
    for (uint64_t i = 0; i < num_bits; ++i) {
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
    }
    bitset.free(num_bits, allocator);
  }
}

TEST(MultilayerBitsetTest, Reset) {
  for (uint64_t num_bits = 1; num_bits < (64ULL * 64 * 64 * 64); num_bits *= 2) { // Test up to 4 layers
    metall::v0::kernel::multilayer_bitset<std::allocator<void>> bitset;
    auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<void>>::rebind_allocator_type();
    bitset.allocate(num_bits, allocator);
    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.find_and_set(num_bits);
    }

    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.reset(num_bits, i);
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
    }

    bitset.free(num_bits, allocator);
  }
}

void RandomAccessHelper(const std::size_t num_bits) {
  metall::v0::kernel::multilayer_bitset<std::allocator<void>> bitset;
  auto allocator = typename metall::v0::kernel::multilayer_bitset<std::allocator<void>>::rebind_allocator_type();
  bitset.allocate(num_bits, allocator);

  std::vector<bool> reference(num_bits, false);

  std::mt19937 mt;
  std::uniform_int_distribution<uint64_t> rand_dist(0, num_bits * 2);

  uint64_t min_free_pos = 0;
  for (uint64_t i = 0; i < num_bits * 8ULL; ++i) { // 8 is just a random number
    const auto random_value = rand_dist(mt);
    const bool do_set = random_value / num_bits;

    if (do_set && !reference[min_free_pos]) { // Set
      ASSERT_EQ(bitset.find_and_set(num_bits), min_free_pos);
      reference[min_free_pos] = true;

      // Make sure that is the minimum available spot
      for (uint64_t p = 0; p <= min_free_pos; ++p) {
        ASSERT_TRUE(reference[p]);
      }

      // Find the next available spot
      for (uint64_t p = min_free_pos + 1; p < reference.size(); ++p) {
        if (!reference[p]) {
          min_free_pos = p;
          break;
        }
      }

    } else if (!do_set) { // Reset
      const uint64_t pos = random_value % num_bits;
      if (!reference[pos]) continue;
      bitset.reset(num_bits, pos);
      reference[pos] = false;
      min_free_pos = std::min(min_free_pos, pos);
    }
  }

  bitset.free(num_bits, allocator);
}

TEST(MultilayerBitsetTest, RandomOperation) {

  // 1 layer
  RandomAccessHelper(1ULL << 0);
  RandomAccessHelper(1ULL << 1);
  RandomAccessHelper(1ULL << 2);
  RandomAccessHelper(1ULL << 3);
  RandomAccessHelper(1ULL << 4);
  RandomAccessHelper(1ULL << 5);
  RandomAccessHelper(1ULL << 6);

  // 2 layer
  RandomAccessHelper(1ULL << 7);
  RandomAccessHelper(1ULL << 10);
  RandomAccessHelper(1ULL << 12);

  // 3 layer
//  RandomAccessHelper(1ULL << 13);
//  RandomAccessHelper(1ULL << 16);
//  RandomAccessHelper(1ULL << 18);

  // 4 layer
//  RandomAccessHelper(1ULL << 19);
//  RandomAccessHelper(1ULL << 22);
//  RandomAccessHelper(1ULL << 24);
}
}
