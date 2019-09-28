// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <string>
#include <algorithm>
#include <random>

#include <boost/container/vector.hpp>

#include <metall/metall.hpp>
#include <metall/detail/utility/file.hpp>

#include "../test_utility.hpp"

namespace {

std::string original_dir_path() {
  const std::string path(test_utility::get_test_dir() + "SnapshotTest");
  return path;
}

std::string snapshot_dir_path() {
  const std::string path(test_utility::get_test_dir() + "SnapshotTest_snapshot");
  return path;
}

TEST(SnapshotTest, Snapshot) {
  metall::manager::remove(original_dir_path().c_str());
  metall::manager::remove(snapshot_dir_path().c_str());

  metall::manager manager(metall::create_only, original_dir_path().c_str());

  [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
  [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

  ASSERT_TRUE(manager.snapshot(snapshot_dir_path().c_str()));
}

TEST(SnapshotTest, Open) {
  metall::manager manager(metall::open_only, snapshot_dir_path().c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

// -------------------------------------------------------------------------------- //
// Randomly update some spots in a contiguous region multiple times
// -------------------------------------------------------------------------------- //
using base_vec_t = boost::container::vector<char>;
using metall_vec_t = boost::container::vector<char, metall::manager::allocator_type<char>>;

base_vec_t ref_vec;

template <typename vec_t>
void random_update(const unsigned int seed, vec_t *vec) {
  std::mt19937_64 mt(seed);
  std::uniform_int_distribution<uint64_t> rand_dist(0, vec->size());

  const ssize_t page_size = metall::detail::utility::get_page_size();
  ASSERT_GT(page_size, 0);

  // Update some of pages
  for (uint64_t i = 0; i < vec->size() / page_size / 8; ++i) {
    const uint64_t index = rand_dist(mt);

    char val = static_cast<char>(index);
    if (val == 0) val = 1; // 0 is used to present that the spot is not updated

    (*vec)[index] = val;
  }
}

template <class input_itr, class ref_input_itr>
void equal(input_itr first, input_itr last, ref_input_itr ref_first) {
  for (; first != last; ++first, ++ref_first) {
    if (*ref_first == 0) continue;
    ASSERT_EQ(*first, *ref_first);
  }
}
}