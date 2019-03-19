// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <metall/manager.hpp>

#include <metall/detail/utility/file.hpp>

namespace {

TEST(SnapshotTest, SparseCopy) {
  metall::manager manager(metall::create_only, "/tmp/snapshot_test_file", metall::detail::k_v0_chunk_size);

  manager.construct<uint8_t>(metall::unique_instance)(1);
  manager.construct<uint16_t>(metall::unique_instance)(1);
  manager.construct<uint32_t>(metall::unique_instance)(1);
  manager.construct<uint64_t>(metall::unique_instance)(1);

  metall::manager::remove_file("/tmp/snapshot_test_file_snapshot");
  manager.snapshot("/tmp/snapshot_test_file_snapshot");

  EXPECT_EQ(metall::detail::utility::get_file_size("/tmp/snapshot_test_file_segment"),
            metall::detail::utility::get_file_size("/tmp/snapshot_test_file_snapshot_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size("/tmp/snapshot_test_file_segment"),
            metall::detail::utility::get_actual_file_size("/tmp/snapshot_test_file_snapshot_segment"));
}
}