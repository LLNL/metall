// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <metall/container/experimental/string_container/string_table.hpp>
#include "../../test_utility.hpp"

namespace {

TEST(ConcurrentMapTest, SequentialInsert) {
  using string_table_type =
      metall::container::experimental::string_container::string_table<
          std::allocator<std::byte>>;

  string_table_type string_table;

  const std::string str1 = "string1";
  const std::string str2 = "string2";
  const std::string str3 = "string3";

  const auto key1 = string_table.insert(str1);
  const auto key2 = string_table.insert(str2.c_str());
  const auto key3 = string_table.insert("string3");

  ASSERT_EQ(string_table.to_key(key1), str1);
  ASSERT_EQ(string_table.to_key(key2), str2);
  ASSERT_EQ(string_table.to_key(key3), str3);
}

}  // namespace