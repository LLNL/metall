// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <string>
#include <filesystem>

#include <metall/metall.hpp>

#include "../test_utility.hpp"

namespace {

namespace fs = std::filesystem;

fs::path original_dir_path() {
  const fs::path path(test_utility::make_test_path("original"));
  return path;
}

fs::path snapshot_dir_path() {
  const fs::path path(test_utility::make_test_path("snapshot"));
  return path;
}

TEST(SnapshotTest, Snapshot) {
  metall::manager::remove(original_dir_path());
  metall::manager::remove(snapshot_dir_path());

  {
    metall::manager manager(metall::create_only, original_dir_path());

    [[maybe_unused]] auto a = manager.construct<uint32_t>("a")(1);
    [[maybe_unused]] auto b =
        manager.construct<uint64_t>(metall::unique_instance)(2);

    ASSERT_TRUE(manager.snapshot(snapshot_dir_path()));
    ASSERT_TRUE(metall::manager::consistent(snapshot_dir_path()));

    // UUID
    const auto original_uuid =
        metall::manager::get_uuid(original_dir_path());
    ASSERT_FALSE(original_uuid.empty());
    const auto snapshot_uuid =
        metall::manager::get_uuid(snapshot_dir_path());
    ASSERT_FALSE(snapshot_uuid.empty());
    ASSERT_NE(original_uuid, snapshot_uuid);

    // Version
    ASSERT_EQ(metall::manager::get_version(original_dir_path()),
              metall::manager::get_version(snapshot_dir_path()));
  }

  {
    metall::manager manager(metall::open_read_only,
                            snapshot_dir_path().c_str());

    auto a = manager.find<uint32_t>("a").first;
    ASSERT_EQ(*a, 1);

    auto b = manager.find<uint64_t>(metall::unique_instance).first;
    ASSERT_EQ(*b, 2);
  }
}
}  // namespace