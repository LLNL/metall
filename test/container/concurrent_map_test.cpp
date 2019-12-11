// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall_container/concurrent_map.hpp>

#include "gtest/gtest.h"
#include <boost/container/map.hpp>
#include <metall_container/concurrent_map.hpp>

namespace {

TEST (ConcurrentMapTest, SequentialInsert) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  const auto v1_2 = std::make_pair('a', 0);
  GTEST_ASSERT_EQ(ref_map.insert(v1).second, map.insert(v1));
  GTEST_ASSERT_EQ(ref_map.insert(v1).second, map.insert(v1)); // Check unique insertion
  GTEST_ASSERT_EQ(ref_map.insert(v1_2).second, map.insert(v1_2)); // Duplicate key

  const auto v2 = std::make_pair('b', 1);
  GTEST_ASSERT_EQ(ref_map.insert(v2).second, map.insert(v2)); // Another key
}

TEST (ConcurrentMapTest, Count) {
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  GTEST_ASSERT_EQ(map.count(v1.first), 0);
  map.insert(v1);
  GTEST_ASSERT_EQ(map.count(v1.first), 1);

  const auto v2 = std::make_pair('b', 2);
  GTEST_ASSERT_EQ(map.count(v2.first), 0);
  map.insert(v2);
  GTEST_ASSERT_EQ(map.count(v2.first), 1);
}

TEST (ConcurrentMapTest, SequentialEdit) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  const auto v2 = std::make_pair('b', 1);
  GTEST_ASSERT_EQ(ref_map.insert(v1).second, map.insert(v1));
  GTEST_ASSERT_EQ(ref_map.insert(v2).second, map.insert(v2));

  const auto v3 = std::make_pair('c', 2);
  ref_map[v3.first] = v3.second;
  {
    auto ret = map.scoped_edit(v3.first);
    ret.first = v3.second;
  }
  GTEST_ASSERT_EQ(ref_map.count(v3.first), map.count(v3.first));

  const auto v4 = std::make_pair('d', 3);
  ref_map[v4.first] = v4.second;
  map.edit(v4.first, [&v4](int &v){
    v = v4.second;
  });
  GTEST_ASSERT_EQ(ref_map.count(v4.first), map.count(v4.first));

}
}