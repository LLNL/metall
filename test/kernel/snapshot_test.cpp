// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <string>

#include <metall/metall.hpp>

#include "../test_utility.hpp"

namespace {

std::string original_dir_path() {
  const std::string path(test_utility::make_test_path("original"));
  return path;
}

std::string snapshot_dir_path() {
  const std::string path(test_utility::make_test_path("snapshot"));
  return path;
}

TEST(SnapshotTest, Snapshot) {

  metall::manager::remove(original_dir_path().c_str());
  metall::manager::remove(snapshot_dir_path().c_str());

  {
    metall::manager manager(metall::create_only, original_dir_path().c_str());

    [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
    [[maybe_unused]] auto b = manager.construct<uint64_t>("b")(2);

    ASSERT_TRUE(manager.snapshot(snapshot_dir_path().c_str()));
    ASSERT_TRUE(metall::manager::consistent(snapshot_dir_path().c_str()));

    // UUID
    const auto original_uuid = metall::manager::get_uuid(original_dir_path().c_str());
    ASSERT_FALSE(original_uuid.empty());
    const auto snapshot_uuid = metall::manager::get_uuid(snapshot_dir_path().c_str());
    ASSERT_FALSE(snapshot_uuid.empty());
    ASSERT_NE(original_uuid, snapshot_uuid);

    // Version
    ASSERT_EQ(metall::manager::get_version(original_dir_path().c_str()),
              metall::manager::get_version(snapshot_dir_path().c_str()));
  }

  {
    metall::manager manager(metall::open_read_only, snapshot_dir_path().c_str());

    auto a = manager.find<uint32_t>("a").first;
    ASSERT_EQ(*a, 1);

    auto b = manager.find<uint64_t>("b").first;
    ASSERT_EQ(*b, 2);
  }
}
}