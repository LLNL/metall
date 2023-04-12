// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/json/json.hpp>

namespace mj = metall::json;

namespace {
using array_type = mj::array<std::allocator<std::byte>>;

TEST(JSONArrayTest, Constructor) {
  array_type array;
  array_type array_with_alloc(std::allocator<std::byte>{});
  array_type cp(array);
  array_type mv(std::move(array));
}

TEST(JSONArrayTest, Size) {
  array_type array;
  GTEST_ASSERT_EQ(array.size(), 0);
  array.resize(10);
  GTEST_ASSERT_EQ(array.size(), 10);
  array.resize(0);
  GTEST_ASSERT_EQ(array.size(), 0);
}

TEST(JSONArrayTest, Capacity) {
  array_type array;
  GTEST_ASSERT_EQ(array.capacity(), 0);
  array.resize(10);
  GTEST_ASSERT_GE(array.capacity(), 10);
  array.resize(0);
  GTEST_ASSERT_GE(array.capacity(), 10);
}

TEST(JSONArrayTest, Bracket) {
  array_type array;
  array.resize(2);
  array[0] = 0;
  array[1] = 1;
  GTEST_ASSERT_EQ(array[0], 0);
  GTEST_ASSERT_EQ(array[1], 1);
}

TEST(JSONArrayTest, Iterator) {
  array_type array;
  array.resize(2);

  int i = 0;
  for (auto itr = array.begin(), end = array.end(); itr != end; ++itr) {
    *itr = i;
    ++i;
  }

  const array_type cnst_array = array;
  i = 0;
  for (const auto &elem : cnst_array) {
    GTEST_ASSERT_EQ(elem, i);
    ++i;
  }
}

TEST(JSONArrayTest, Erase) {
  array_type array;
  array.resize(4);

  array[0] = 0;
  array[1] = 1;
  array[2] = 2;
  array[3] = 3;
  GTEST_ASSERT_EQ(*(array.erase(array.begin())), 1);

  const auto &ref_array = array;
  GTEST_ASSERT_EQ(*(array.erase(ref_array.begin() + 1)), 3);

  GTEST_ASSERT_EQ(array.erase(array.begin()), array.begin());
  auto end = array.erase(array.begin());
  GTEST_ASSERT_EQ(end, array.end());
  GTEST_ASSERT_EQ(array.size(), 0);
}

TEST(JSONArrayTest, EqualOperator) {
  array_type array0;
  array0.resize(2);
  array_type array1;
  array1.resize(2);

  array0[0] = 0;
  array0[1] = 1;
  array1[0] = 0;
  array1[1] = 1;

  GTEST_ASSERT_TRUE(array0 == array1);
  GTEST_ASSERT_FALSE(array0 != array1);
  array0[1] = 2;
  GTEST_ASSERT_TRUE(array0 != array1);
  GTEST_ASSERT_FALSE(array0 == array1);
}

TEST(JSONArrayTest, Swap) {
  array_type array0;
  array0.resize(2);
  array_type array1;
  array1.resize(2);

  array0[0] = 0;
  array0[1] = 1;
  array1[0] = 2;
  array1[1] = 3;

  array0.swap(array1);

  GTEST_ASSERT_EQ(array0[0], 2);
  GTEST_ASSERT_EQ(array0[1], 3);
  GTEST_ASSERT_EQ(array1[0], 0);
  GTEST_ASSERT_EQ(array1[1], 1);
}

TEST(JSONArrayTest, Clear) {
  array_type array;
  array.resize(2);
  array.clear();
  GTEST_ASSERT_EQ(array.size(), 0);
}

TEST(JSONArrayTest, PushBack) {
  array_type array;

  array_type::value_type value0(array.get_allocator());
  value0.emplace_int64() = 0;
  array.push_back(value0);

  array_type::value_type value1(array.get_allocator());
  value1.emplace_string() = "1";
  array.push_back(std::move(value1));

  GTEST_ASSERT_EQ(array[0].as_int64(), 0);
  GTEST_ASSERT_EQ(array[1].as_string(), "1");
}

}  // namespace