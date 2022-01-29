// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/container/experimental/json/json.hpp>

using namespace metall::container::experimental;

namespace {
using array_type = json::array<std::allocator<std::byte>>;

TEST (JSONArrayTest, Constructor) {
  array_type array;
  array_type array_with_alloc(std::allocator<std::byte>{});
  array_type cp(array);
  array_type mv(std::move(array));
}

TEST (JSONArrayTest, Size) {
  array_type array;
  GTEST_ASSERT_EQ(array.size(), 0);
  array.resize(10);
  GTEST_ASSERT_EQ(array.size(), 10);
  array.resize(0);
  GTEST_ASSERT_EQ(array.size(), 0);
}

TEST (JSONArrayTest, Bracket) {
  array_type array;
  array.resize(2);
  array[0] = 0;
  array[1] = 1;
  GTEST_ASSERT_EQ(array[0], 0);
  GTEST_ASSERT_EQ(array[1], 1);
}

TEST (JSONArrayTest, Iterator) {
  array_type array;
  array.resize(2);

  int i = 0;
  for (auto itr = array.begin(), end = array.end(); itr != end; ++itr) {
    *itr = i;
    ++i;
  }

  const array_type cnst_array = array;
  i = 0;
  for (const auto &elem: cnst_array) {
    GTEST_ASSERT_EQ(elem, i);
    ++i;
  }
}

TEST (JSONArrayTest, Erase) {
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

TEST (JSONArrayTest, EqualOperator) {
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
  array0[1]= 2;
  GTEST_ASSERT_TRUE(array0 != array1);
  GTEST_ASSERT_FALSE(array0 == array1);
}
}