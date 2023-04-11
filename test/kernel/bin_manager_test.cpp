// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <limits>

#include <metall/kernel/bin_number_manager.hpp>
#include <metall/kernel/object_size_manager.hpp>
#include <metall/metall.hpp>

namespace {
using namespace metall::kernel::object_size_manager_detail;

constexpr std::size_t k_chunk_size = metall::manager::chunk_size();
constexpr std::size_t k_max_size = 1ULL << 48ULL;
using bin_no_mngr =
    metall::kernel::bin_number_manager<k_chunk_size, k_max_size>;

TEST(BinManagerTest, BinNoType) {
  ASSERT_GE(std::numeric_limits<bin_no_mngr::bin_no_type>::max(),
            bin_no_mngr::to_bin_no(k_max_size));
}

TEST(BinManagerTest, ToSmallBinNo) {
  for (int i = 0;; ++i) {
    if (k_size_table<k_chunk_size, k_max_size>[i] >= k_chunk_size) {
      ASSERT_EQ(i, bin_no_mngr::num_small_bins());
      break;
    }

    if (i > 0) {
      ASSERT_EQ(
          bin_no_mngr::to_bin_no(k_size_table<k_chunk_size, k_max_size>[i] - 1),
          i)
          << i << " == " << k_size_table<k_chunk_size, k_max_size>[i] - 1;
    }
    ASSERT_EQ(bin_no_mngr::to_bin_no(k_size_table<k_chunk_size, k_max_size>[i]),
              i)
        << i << " == " << k_size_table<k_chunk_size, k_max_size>[i];
    ASSERT_EQ(
        bin_no_mngr::to_bin_no(k_size_table<k_chunk_size, k_max_size>[i] + 1),
        i + 1)
        << i << " == " << k_size_table<k_chunk_size, k_max_size>[i] + 1;
  }
}

TEST(BinManagerTest, ToLargeBinNo) {
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size - 1),
            bin_no_mngr::num_small_bins());
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size),
            bin_no_mngr::num_small_bins());
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size + 1),
            bin_no_mngr::num_small_bins() + 1);

  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 2 - 1),
            bin_no_mngr::num_small_bins() + 1);
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 2),
            bin_no_mngr::num_small_bins() + 1);
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 2 + 1),
            bin_no_mngr::num_small_bins() + 2);

  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 3 - 1),
            bin_no_mngr::num_small_bins() + 2);
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 3),
            bin_no_mngr::num_small_bins() + 2);
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_chunk_size * 3 + 1),
            bin_no_mngr::num_small_bins() + 2);

  ASSERT_EQ(bin_no_mngr::to_bin_no(k_max_size - 1),
            bin_no_mngr::num_small_bins() + bin_no_mngr::num_large_bins() - 1);
  ASSERT_EQ(bin_no_mngr::to_bin_no(k_max_size),
            bin_no_mngr::num_small_bins() + bin_no_mngr::num_large_bins() - 1);
}

}  // namespace