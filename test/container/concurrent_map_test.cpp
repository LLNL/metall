// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <filesystem>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/map.hpp>
#include <metall/container/concurrent_map.hpp>
#include <metall/detail/file.hpp>
#include "../test_utility.hpp"

namespace bip = boost::interprocess;

namespace {

TEST(ConcurrentMapTest, SequentialInsert) {
  boost::container::map<char, int> ref_map;
  metall::container::concurrent_map<char, int> map;

  const auto v1 = std::make_pair('a', 0);
  const auto v1_2 = std::make_pair('a', 0);
  GTEST_ASSERT_EQ(ref_map.insert(v1).second, map.insert(v1));
  GTEST_ASSERT_EQ(ref_map.insert(v1).second,
                  map.insert(v1));  // Check unique insertion
  GTEST_ASSERT_EQ(ref_map.insert(v1_2).second,
                  map.insert(v1_2));  // Duplicate key

  const auto v2 = std::make_pair('b', 1);
  GTEST_ASSERT_EQ(ref_map.insert(v2).second, map.insert(v2));  // Another key
}

TEST(ConcurrentMapTest, Count) {
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

TEST(ConcurrentMapTest, Size) {
  metall::container::concurrent_map<char, int> map;

  GTEST_ASSERT_EQ(map.size(), 0);

  map.insert(std::make_pair('a', 0));
  GTEST_ASSERT_EQ(map.size(), 1);

  map.insert(std::make_pair('b', 0));
  GTEST_ASSERT_EQ(map.size(), 2);

  map.insert(std::make_pair('b', 0));
  GTEST_ASSERT_EQ(map.size(), 2);
}

TEST(ConcurrentMapTest, SequentialEdit) {
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
  map.edit(v2.first, [&v2](int& v) { v = v2.second; });
  GTEST_ASSERT_EQ(ref_map.count(v2.first), map.count(v2.first));
}

TEST(ConcurrentMapTest, Find) {
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

TEST(ConcurrentMapTest, Iterator) {
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
    GTEST_ASSERT_EQ(ref_map.count(itr->first), 1)
        << "Invalid key was found: " << (char)itr->first;
    GTEST_ASSERT_EQ(ref_map.at(itr->first), itr->second);
    ++num_elems;
  }
  GTEST_ASSERT_EQ(num_elems, 2);
}

TEST(ConcurrentMapTest, Persistence) {
  using allocator_type =
      bip::allocator<std::pair<const char, int>,
                     bip::managed_mapped_file::segment_manager>;
  using map_type =
      metall::container::concurrent_map<char, int, std::less<char>,
                                        std::hash<char>, allocator_type, 2>;

  const std::filesystem::path file_path(test_utility::make_test_path());

  test_utility::create_test_dir();
  metall::mtlldetail::remove_file(file_path);

  std::vector<std::pair<char, int>> inputs(10);
  for (int i = 0; i < (int)inputs.size(); ++i) {
    inputs[i].first = (char)('a' + i);
    inputs[i].second = i;
  }

  {
    bip::managed_mapped_file mfile(bip::create_only, file_path.c_str(),
                                   1 << 20);
    auto pmap = mfile.construct<map_type>("map")(
        mfile.get_allocator<typename allocator_type::value_type>());

    for (auto& elem : inputs) {
      pmap->insert(elem);
    }
  }

  {
    bip::managed_mapped_file mfile(bip::open_only, file_path.c_str());
    const auto pmap = mfile.find<map_type>("map").first;
    GTEST_ASSERT_NE(pmap, nullptr);

    for (auto& elem : inputs) {
      GTEST_ASSERT_NE(pmap->find(elem.first), pmap->cend());
      auto itr = pmap->find(elem.first);
      GTEST_ASSERT_EQ(itr->first, elem.first);
      GTEST_ASSERT_EQ(itr->second, elem.second);
    }
  }
}
}  // namespace
