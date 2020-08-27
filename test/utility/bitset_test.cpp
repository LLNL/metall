// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <bitset>
#include <random>
#include <metall/detail/utility/bitset.hpp>

namespace {

template <int num_bits>
using block_type = typename metall::detail::utility::bitset_detail::block_type<num_bits>::type;

using metall::detail::utility::bitset_detail::num_blocks;
using metall::detail::utility::bitset_detail::get;
using metall::detail::utility::bitset_detail::set;
using metall::detail::utility::bitset_detail::reset;

TEST(BitsetTest, BaseType) {
  ASSERT_LE(0, sizeof(block_type<0>) * 8);
  ASSERT_LE(1, sizeof(block_type<1>) * 8);
  ASSERT_LE(7, sizeof(block_type<7>) * 8);
  ASSERT_EQ(8, sizeof(block_type<8>) * 8);
  ASSERT_LE(9, sizeof(block_type<9>) * 8);
  ASSERT_LE(15, sizeof(block_type<15>) * 8);
  ASSERT_EQ(16, sizeof(block_type<16>) * 8);
  ASSERT_LE(17, sizeof(block_type<17>) * 8);
  ASSERT_LE(31, sizeof(block_type<31>) * 8);
  ASSERT_EQ(32, sizeof(block_type<32>) * 8);
  ASSERT_LE(33, sizeof(block_type<33>) * 8);
  ASSERT_LE(63, sizeof(block_type<63>) * 8);
  ASSERT_EQ(64, sizeof(block_type<64>) * 8);
  ASSERT_TRUE(sizeof(block_type<65>) == sizeof(uint64_t));
  ASSERT_TRUE(sizeof(block_type<128>) == sizeof(uint64_t));
}

TEST(BitsetTest, BitsetSize) {
  ASSERT_LE(1, num_blocks<block_type<1>>(1) * sizeof(block_type<1>) * 8);

  ASSERT_LE(7, num_blocks<block_type<7>>(7) * sizeof(block_type<7>) * 8);
  ASSERT_EQ(8, num_blocks<block_type<8>>(8) * sizeof(block_type<8>) * 8);
  ASSERT_LE(9, num_blocks<block_type<9>>(9) * sizeof(block_type<9>) * 8);

  ASSERT_LE(15, num_blocks<block_type<15>>(15) * sizeof(block_type<15>) * 8);
  ASSERT_EQ(16, num_blocks<block_type<16>>(16) * sizeof(block_type<16>) * 8);
  ASSERT_LE(17, num_blocks<block_type<17>>(17) * sizeof(block_type<17>) * 8);

  ASSERT_LE(31, num_blocks<block_type<31>>(31) * sizeof(block_type<31>) * 8);
  ASSERT_EQ(32, num_blocks<block_type<32>>(32) * sizeof(block_type<32>) * 8);
  ASSERT_LE(33, num_blocks<block_type<33>>(33) * sizeof(block_type<33>) * 8);

  ASSERT_LE(63, num_blocks<block_type<63>>(63) * sizeof(block_type<63>) * 8);
  ASSERT_EQ(64, num_blocks<block_type<64>>(64) * sizeof(block_type<64>) * 8);
  ASSERT_LE(65, num_blocks<block_type<65>>(65) * sizeof(block_type<65>) * 8);

  ASSERT_LE(127, num_blocks<block_type<127>>(127) * sizeof(block_type<127>) * 8);
  ASSERT_EQ(128, num_blocks<block_type<128>>(128) * sizeof(block_type<128>) * 8);
  ASSERT_LE(129, num_blocks<block_type<129>>(129) * sizeof(block_type<129>) * 8);

  ASSERT_EQ(1ULL << 10ULL, num_blocks<block_type<1ULL << 10ULL>>(1ULL << 10ULL) * sizeof(block_type<1ULL << 10ULL>) * 8);
  ASSERT_EQ(1ULL << 20ULL, num_blocks<block_type<1ULL << 20ULL>>(1ULL << 20ULL) * sizeof(block_type<1ULL << 20ULL>) * 8);
  ASSERT_EQ(1ULL << 30ULL, num_blocks<block_type<1ULL << 30ULL>>(1ULL << 30ULL) * sizeof(block_type<1ULL << 30ULL>) * 8);
}

template <std::size_t num_bits>
void RandomAccessHelper()  {
  std::bitset<num_bits> reference;
  auto bitset = new block_type<num_bits>[num_blocks<block_type<num_bits>>(num_bits)](); // () is for zero initialization

  std::mt19937_64 mt;
  std::uniform_int_distribution<uint64_t> rand_dist(0, num_bits * 2);

  for (uint64_t i = 0; i < num_bits * 8; ++i) { // 8 is just a random number
    const auto random_value = rand_dist(mt);
    const uint64_t pos = random_value % num_bits;
    const bool do_set = random_value / num_bits;
    if (do_set) {
      set(bitset, pos);
      reference.set(pos);
    } else {
      reset(bitset, pos);
      reference.reset(pos);
    }

    // Validation
    for (uint64_t k = 0; k < num_bits; ++k) {
      ASSERT_EQ(get(bitset, k), reference[k]);
    }
  }
  delete[] bitset;
}

TEST(BitsetTest, RandomAccess) {
  RandomAccessHelper<8>();
  RandomAccessHelper<16>();
  RandomAccessHelper<32>();
  RandomAccessHelper<64>();
  RandomAccessHelper<128>();
  RandomAccessHelper<1 << 10>();
  // RandomAccessHelper<1 << 20>();
  // RandomAccessHelper<1 << 30>();
}
}