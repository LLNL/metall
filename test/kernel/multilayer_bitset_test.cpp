// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <cmath>
#include <vector>
#include <random>
#include <unordered_set>
#include <cstddef>

#include <metall/detail/bitset.hpp>
#include <metall/kernel/multilayer_bitset.hpp>

namespace {

TEST(MultilayerBitsetTest, FindAndSet) {
  for (uint64_t num_bits = 1; num_bits <= (64ULL * 64 * 64 * 32);
       num_bits *= 64) {  // Test up to 4 layers,
    metall::kernel::multilayer_bitset bitset;
    bitset.allocate(num_bits);
    for (uint64_t i = 0; i < num_bits; ++i) {
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
      ASSERT_TRUE(bitset.get(num_bits, i));
    }
    bitset.free(num_bits);
  }
}

TEST(MultilayerBitsetTest, Reset) {
  for (uint64_t num_bits = 1; num_bits <= (64ULL * 64 * 64 * 32);
       num_bits *= 64) {  // Test up to 4 layers
    metall::kernel::multilayer_bitset bitset;
    bitset.allocate(num_bits);
    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.find_and_set(num_bits);
    }

    for (uint64_t i = 0; i < num_bits; ++i) {
      bitset.reset(num_bits, i);
      ASSERT_FALSE(bitset.get(num_bits, i));
      ASSERT_EQ(bitset.find_and_set(num_bits), i);
    }

    bitset.free(num_bits);
  }
}

void FindAndSetManyHelper(
    const std::size_t num_bits, const std::size_t num_to_find,
    metall::kernel::multilayer_bitset &bitset,
    std::unordered_set<metall::kernel::multilayer_bitset::bit_position_type>
        &used_bits) {
  SCOPED_TRACE("#of bits = " + std::to_string(num_bits) +
               ", #of finds = " + std::to_string(num_to_find));

  metall::kernel::multilayer_bitset::bit_position_type buf[num_to_find];

  bitset.find_and_set_many(num_bits, num_to_find, buf);

  for (std::size_t i = 0; i < num_to_find; ++i) {
    used_bits.insert(buf[i]);
  }

  for (uint64_t i = 0; i < num_bits; ++i) {
    if (used_bits.count(i))
      ASSERT_TRUE(bitset.get(num_bits, i)) << " i = " << i;
    else
      ASSERT_FALSE(bitset.get(num_bits, i)) << " i = " << i;
  }
}

TEST(MultilayerBitsetTest, FindAndSetMany) {
  for (std::size_t num_bits = 1; num_bits <= (64ULL * 64 * 64 * 32);
       num_bits *= 64) {  // Test up to 4 layers,
    metall::kernel::multilayer_bitset bitset;
    bitset.allocate(num_bits);

    std::unordered_set<metall::kernel::multilayer_bitset::bit_position_type>
        used_bits;

    FindAndSetManyHelper(num_bits, 1, bitset, used_bits);

    if (num_bits < 1 + 64) continue;
    FindAndSetManyHelper(num_bits, 64, bitset, used_bits);

    if (num_bits < 1 + 64 + 128) continue;
    FindAndSetManyHelper(num_bits, 128, bitset, used_bits);

    bitset.free(num_bits);
  }
}

enum mode : uint8_t { set, reset, set_many };

void RandomSetAndResetHelper2(const std::size_t num_bits) {
  SCOPED_TRACE("num_bits = " + std::to_string(num_bits));

  metall::kernel::multilayer_bitset bitset;
  bitset.allocate(num_bits);

  std::vector<bool> reference(num_bits, false);

  std::random_device rd;
  std::mt19937_64 rnd_gen(rd());
  std::uniform_int_distribution<uint64_t> dist(0, num_bits - 1);

  std::discrete_distribution<> mode_dist(
      {mode::set, mode::set, mode::reset, mode::reset, mode::set_many});

  std::size_t cnt_trues = 0;
  for (std::size_t i = 0; i < num_bits * 2ULL; ++i) {  // Just repeat many times
    const auto mode = mode_dist(rnd_gen);

    if (mode == mode::set) {
      const auto pos = bitset.find_and_set(num_bits);
      if (!reference[pos]) {
        ++cnt_trues;
        reference[pos] = true;
      }
    } else if (mode == mode::reset) {
      const auto position = dist(rnd_gen);
      ASSERT_EQ(bitset.get(num_bits, position), reference[position]);
      bitset.reset(num_bits, position);
      if (reference[position]) {
        --cnt_trues;
        reference[position] = false;
      }
    } else {
      const auto n = std::min((size_t)dist(rnd_gen), num_bits - cnt_trues);
      metall::kernel::multilayer_bitset::bit_position_type buf[n];
      bitset.find_and_set_many(num_bits, n, buf);
      cnt_trues += n;
      for (std::size_t i = 0; i < n; ++i) {
        ASSERT_FALSE(reference[buf[i]]) << i << " " << buf[i];
        reference[buf[i]] = true;
      }
    }

    if (num_bits < 10 || i % (num_bits / 10) == 0) {
      for (std::size_t pos = 0; pos < num_bits; ++pos) {
        ASSERT_EQ(bitset.get(num_bits, pos), reference[pos]);
      }
    }
  }

  {
    std::size_t cnt = 0;
    for (std::size_t i = 0; i < reference.size(); ++i) {
      cnt += reference[i] ? 1 : 0;
    }
    assert(cnt == cnt_trues);
  }

  // Set the remaining bits
  {
    const auto num_rem = num_bits - cnt_trues;
    metall::kernel::multilayer_bitset::bit_position_type buf[num_rem];
    bitset.find_and_set_many(num_bits, num_rem, buf);
  }
  for (std::size_t pos = 0; pos < num_bits; ++pos) {
    ASSERT_TRUE(bitset.get(num_bits, pos)) << "pos = " << pos;
  }

  bitset.free(num_bits);
}
}  // namespace

TEST(MultilayerBitsetTest, RandomSetAndReset2) {
  // 1–2 layers
  for (std::size_t num_bits = 1; num_bits <= 64 * 4; ++num_bits) {
    RandomSetAndResetHelper2(num_bits);
  }

  // 2 layers
  RandomSetAndResetHelper2(64 * 64 - 1);
  RandomSetAndResetHelper2(64 * 64);

  // 3 layers
  RandomSetAndResetHelper2(64 * 64 + 1);
  RandomSetAndResetHelper2(64 * 64 * 64 - 1);
  RandomSetAndResetHelper2(64 * 64 * 64);

  // 4 layers
  RandomSetAndResetHelper2(64 * 64 * 64 + 1);
}