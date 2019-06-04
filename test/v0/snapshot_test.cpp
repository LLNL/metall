// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <string>

#include <metall/metall.hpp>
#include <metall/detail/utility/file.hpp>

namespace {

const char * k_origin_path = "/tmp/snapshot_test_file";
const char * k_snapshot_path = "/tmp/snapshot_test_file_snapshot";

TEST(SnapshotTest, Snapshot) {
  metall::manager::remove_file(k_origin_path);
  metall::manager::remove_file(k_snapshot_path);

  metall::manager manager(metall::create_only, k_origin_path, metall::manager::chunk_size() * 2);

  [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
  [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

  ASSERT_TRUE(manager.snapshot(k_snapshot_path));

  EXPECT_EQ(metall::detail::utility::get_file_size(std::string(k_origin_path) + "_segment"),
            metall::detail::utility::get_file_size(std::string(k_snapshot_path) + "_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size(std::string(k_origin_path) + "_segment"),
            metall::detail::utility::get_actual_file_size(std::string(k_snapshot_path) + "_segment"));
}

TEST(SnapshotTest, Open) {
  metall::manager manager(metall::open_only, k_snapshot_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

TEST(SnapshotTest, SnapshotDiff0) {
  metall::manager::remove_file(k_origin_path);
  metall::manager::remove_file(k_snapshot_path);

  metall::manager manager(metall::create_only, k_origin_path, metall::manager::chunk_size() * 3);

  [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
  [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

  ASSERT_TRUE(manager.snapshot_diff(k_snapshot_path));

  EXPECT_EQ(metall::detail::utility::get_file_size(std::string(k_origin_path) + "_segment"),
            metall::detail::utility::get_file_size(std::string(k_snapshot_path) + "_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size(std::string(k_origin_path) + "_segment"),
            metall::detail::utility::get_actual_file_size(std::string(k_snapshot_path) + "_segment"));
}

TEST(SnapshotTest, SnapshotDiff1) {
  metall::manager manager(metall::open_only, k_snapshot_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);
  *a = 3;

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
  *b = 4;

  ASSERT_TRUE(manager.snapshot_diff(k_snapshot_path));
}

TEST(SnapshotTest, SnapshotDiff2) {
  metall::manager manager(metall::open_only, k_snapshot_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 3);
  *a = 5;

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 4);
  *b = 6;

  [[maybe_unused]] auto c = manager.construct<uint8_t>("c")(7);

  ASSERT_TRUE(manager.snapshot_diff(k_snapshot_path));
}

TEST(SnapshotTest, OpenDiff) {
  metall::manager manager(metall::open_only, k_snapshot_path);

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 5);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 6);

  auto c = manager.find<uint8_t>("c").first;
  ASSERT_EQ(*c, 7);
}
}