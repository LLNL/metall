// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

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

  const auto v2 = std::make_pair('b', 1);
  GTEST_ASSERT_EQ(map.count(v2.first), 0);
  map.insert(v2);
  GTEST_ASSERT_EQ(map.count(v2.first), 1);
}

TEST (ConcurrentMapTest, SequentialEdit) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  ref_map[v1.first] = v1.second;
  {
    auto ret = map.scoped_edit(v1.first);
    ret.first = v1.second;
  }
  GTEST_ASSERT_EQ(ref_map.count(v1.first), map.count(v1.first));

  const auto v2 = std::make_pair('b', 1);
  ref_map[v2.first] = v2.second;
  map.edit(v2.first, [&v2](int &v){
    v = v2.second;
  });
  GTEST_ASSERT_EQ(ref_map.count(v2.first), map.count(v2.first));
}

TEST (ConcurrentMapTest, Find) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  GTEST_ASSERT_EQ(map.find(v1.first), map.cend());
  map.insert(v1);
  GTEST_ASSERT_NE(map.find(v1.first), map.cend());
  auto itr1 = map.find(v1.first);
  GTEST_ASSERT_EQ(itr1->first, v1.first);
  GTEST_ASSERT_EQ(itr1->second, v1.second);

  const auto v2 = std::make_pair('b', 1);
  GTEST_ASSERT_EQ(map.find(v2.first), map.cend());
  map.insert(v2);
  GTEST_ASSERT_NE(map.find(v2.first), map.cend());
  auto itr2 = map.find(v2.first);
  GTEST_ASSERT_EQ(itr2->first, v2.first);
  GTEST_ASSERT_EQ(itr2->second, v2.second);
}

TEST (ConcurrentMapTest, Iterator) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  ref_map.insert(v1);
  map.insert(v1);

  const auto v2 = std::make_pair('b', 1);
  ref_map.insert(v2);
  map.insert(v2);

  int num_elems = 0;
  for (auto itr = map.cbegin(), end = map.cend(); itr != end; ++itr) {
    GTEST_ASSERT_EQ(ref_map.count(itr->first), 1) << "Invalid key was found: " << (char)itr->first;
    GTEST_ASSERT_EQ(ref_map.at(itr->first), itr->second);
    ++num_elems;
  }
  GTEST_ASSERT_EQ(num_elems, 2);
}
}
