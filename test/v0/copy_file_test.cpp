// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <metall/metall.hpp>

#include <metall/detail/utility/file.hpp>

namespace {

void open() {
  metall::manager manager(metall::open_only, "/tmp/copy_file_test_file_copy");

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

TEST(CopyFileTest, SyncCopy) {
  metall::manager::remove_file("/tmp/copy_file_test_file");
  metall::manager::remove_file("/tmp/copy_file_test_file_copy");

  {
    metall::manager manager(metall::create_only, "/tmp/copy_file_test_file", metall::manager::chunk_size() * 2);

    manager.construct<uint32_t>("a")(1);

    manager.construct<uint64_t>("b")(2);

    manager.sync();

    ASSERT_TRUE(metall::manager::copy_file("/tmp/copy_file_test_file", "/tmp/copy_file_test_file_copy"));

    EXPECT_EQ(metall::detail::utility::get_file_size("/tmp/copy_file_test_file_segment"),
              metall::detail::utility::get_file_size("/tmp/copy_file_test_file_copy_segment"));

    EXPECT_EQ(metall::detail::utility::get_actual_file_size("/tmp/copy_file_test_file_segment"),
              metall::detail::utility::get_actual_file_size("/tmp/copy_file_test_file_copy_segment"));
  }
  open();
}

TEST(CopyFileTest, AsyncCopy) {
  metall::manager::remove_file("/tmp/copy_file_test_file");
  metall::manager::remove_file("/tmp/copy_file_test_file_copy");

  {
    metall::manager manager(metall::create_only, "/tmp/copy_file_test_file", metall::manager::chunk_size() * 2);

    manager.construct<uint32_t>("a")(1);

    manager.construct<uint64_t>("b")(2);

    manager.sync();

    auto handler = metall::manager::copy_file_async("/tmp/copy_file_test_file", "/tmp/copy_file_test_file_copy");
    ASSERT_TRUE(handler.get());

    EXPECT_EQ(metall::detail::utility::get_file_size("/tmp/copy_file_test_file_segment"),
              metall::detail::utility::get_file_size("/tmp/copy_file_test_file_copy_segment"));

    EXPECT_EQ(metall::detail::utility::get_actual_file_size("/tmp/copy_file_test_file_segment"),
              metall::detail::utility::get_actual_file_size("/tmp/copy_file_test_file_copy_segment"));
  }
  open();
}
}