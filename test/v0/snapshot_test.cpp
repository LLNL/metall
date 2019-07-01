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

const std::string& original_file_path() {
  const static std::string path(test_utility::test_file_path("SnapshotTest"));
  return path;
}

const std::string& snapshot_file_path() {
  const static std::string path(test_utility::test_file_path("SnapshotTest"));
  return path;
}

TEST(SnapshotTest, Snapshot) {
  metall::manager::remove_file(original_file_path().c_str());
  metall::manager::remove_file(snapshot_file_path().c_str());

  metall::manager manager(metall::create_only, original_file_path().c_str(), metall::manager::chunk_size() * 2);

  [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
  [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

  ASSERT_TRUE(manager.snapshot(snapshot_file_path().c_str()));

  // Check sparse file copy
  // Could fail if the underling file system does not support sparse copy
  EXPECT_EQ(metall::detail::utility::get_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_file_size(snapshot_file_path() + "_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_actual_file_size(snapshot_file_path() + "_segment"));
}

TEST(SnapshotTest, Open) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

TEST(SnapshotTest, SnapshotDiff0) {
  metall::manager::remove_file(original_file_path().c_str());
  metall::manager::remove_file(snapshot_file_path().c_str());

  metall::manager manager(metall::create_only, original_file_path().c_str(), metall::manager::chunk_size() * 3);

  [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
  [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));

  // Check sparse file copy
  // Could fail if the underling file system does not support sparse copy
  EXPECT_EQ(metall::detail::utility::get_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_file_size(snapshot_file_path() + "_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_actual_file_size(snapshot_file_path() + "_segment"));
}

TEST(SnapshotTest, SnapshotDiff1) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);
  *a = 3;

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
  *b = 4;

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));
}

TEST(SnapshotTest, SnapshotDiff2) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 3);
  *a = 5;

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 4);
  *b = 6;

  [[maybe_unused]] auto c = manager.construct<uint8_t>("c")(7);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));
}

TEST(SnapshotTest, OpenDiff) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 5);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 6);

  auto c = manager.find<uint8_t>("c").first;
  ASSERT_EQ(*c, 7);
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

TEST(SnapshotTest, RandomSnapshotDiff0) {
  metall::manager::remove_file(original_file_path().c_str());
  metall::manager::remove_file(snapshot_file_path().c_str());

  metall::manager manager(metall::create_only, original_file_path().c_str(), metall::manager::chunk_size() * 16);
  auto pvec = manager.construct<metall_vec_t>("vec")(manager.get_allocator<>());

  ref_vec.resize(metall::manager::chunk_size() * 2);
  pvec->resize(metall::manager::chunk_size() * 2);

  std::fill(ref_vec.begin(), ref_vec.end(), 0);
  random_update(11, &ref_vec);
  random_update(11, pvec);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));

  // Check sparse file copy
  // Could fail if the underling file system does not support sparse copy
  EXPECT_EQ(metall::detail::utility::get_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_file_size(snapshot_file_path() + "_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size(original_file_path() + "_segment"),
            metall::detail::utility::get_actual_file_size(snapshot_file_path() + "_segment"));
}

// Update some spots
TEST(SnapshotTest, RandomSnapshotDiff1) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto pvec = manager.find<metall_vec_t>("vec").first;

  ASSERT_EQ(pvec->size(), ref_vec.size());
  equal(pvec->begin(), pvec->end(), ref_vec.begin());

  random_update(22, &ref_vec);
  random_update(22, pvec);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));
}

// Grow the region and update some spots
TEST(SnapshotTest, RandomSnapshotDiff2) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto pvec = manager.find<metall_vec_t>("vec").first;

  ASSERT_EQ(pvec->size(), ref_vec.size());
  equal(pvec->begin(), pvec->end(), ref_vec.begin());

  ref_vec.resize(ref_vec.size() * 2, 0);
  pvec->resize(pvec->size() * 2);

  random_update(33, &ref_vec);
  random_update(33, pvec);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));
}

// Shrink the region and update some spots
TEST(SnapshotTest, RandomSnapshotDiff3) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto pvec = manager.find<metall_vec_t>("vec").first;

  ASSERT_EQ(pvec->size(), ref_vec.size());
  equal(pvec->begin(), pvec->end(), ref_vec.begin());

  ref_vec.resize(ref_vec.size() / 2);
  pvec->resize(pvec->size() / 2);

  random_update(44, &ref_vec);
  random_update(44, pvec);

  ASSERT_TRUE(manager.snapshot_diff(snapshot_file_path().c_str()));
}

// Final step, just check values
TEST(SnapshotTest, RandomSnapshotDiff4) {
  metall::manager manager(metall::open_only, snapshot_file_path().c_str());

  auto pvec = manager.find<metall_vec_t>("vec").first;

  ASSERT_EQ(pvec->size(), ref_vec.size());
  equal(pvec->begin(), pvec->end(), ref_vec.begin());
}
}