// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <metall/manager.hpp>

#include <metall/detail/utility/file.hpp>

namespace {

TEST(SnapshotTest, SparseCopy) {
  metall::manager manager(metall::create_only, "/tmp/snapshot_test_file", metall::manager::chunk_size() * 2);

  uint64_t *a = static_cast<uint64_t *>(manager.allocate(sizeof(uint64_t)));
  *a = 0;

  uint64_t *b = static_cast<uint64_t *>(manager.allocate(sizeof(uint64_t) * 2));
  b[0] = 0;
  b[1] = 0;

  ASSERT_TRUE(manager.snapshot("/tmp/snapshot_test_file_snapshot"));

  EXPECT_EQ(metall::detail::utility::get_file_size("/tmp/snapshot_test_file_segment"),
            metall::detail::utility::get_file_size("/tmp/snapshot_test_file_snapshot_segment"));

  EXPECT_EQ(metall::detail::utility::get_actual_file_size("/tmp/snapshot_test_file_segment"),
            metall::detail::utility::get_actual_file_size("/tmp/snapshot_test_file_snapshot_segment"));

  ASSERT_TRUE(metall::manager::remove_file("/tmp/snapshot_test_file_snapshot"));
}
}