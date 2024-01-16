// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <metall/metall.hpp>

#include <metall/detail/file.hpp>
#include "../test_utility.hpp"

namespace {

void create(const std::string &dir_path) {
  metall::manager manager(metall::create_only, dir_path.c_str());

  manager.construct<uint32_t>("a")(1);
  manager.construct<uint64_t>("b")(2);
}

void modify(const std::string &dir_path) {
  metall::manager manager(metall::open_only, dir_path.c_str());

  auto a = manager.find<uint32_t>("a").first;
  *a = 10;
  auto b = manager.find<uint64_t>("b").first;
  *b = 20;
}

void open(const std::string &dir_path) {
  metall::manager manager(metall::open_read_only, dir_path.c_str());

  auto a = manager.find<uint32_t>("a").first;
  ASSERT_EQ(*a, 1);

  auto b = manager.find<uint64_t>("b").first;
  ASSERT_EQ(*b, 2);
}

const std::string &original_dir_path() {
  const static std::string path(test_utility::make_test_path("/original"));
  return path;
}

const std::string &copy_dir_path() {
  const static std::string path(test_utility::make_test_path("/copy"));
  return path;
}

TEST(CopyFileTest, SyncCopy) {
  metall::manager::remove(original_dir_path().c_str());
  metall::manager::remove(copy_dir_path().c_str());

  create(original_dir_path());

  ASSERT_TRUE(metall::manager::copy(original_dir_path().c_str(),
                                    copy_dir_path().c_str()));

  modify(original_dir_path());

  open(copy_dir_path());
}

TEST(CopyFileTest, AsyncCopy) {
  metall::manager::remove(original_dir_path().c_str());
  metall::manager::remove(copy_dir_path().c_str());

  create(original_dir_path());

  auto handler = metall::manager::copy_async(original_dir_path().c_str(),
                                             copy_dir_path().c_str());
  ASSERT_TRUE(handler.get());

  modify(original_dir_path());

  open(copy_dir_path());
}
}  // namespace