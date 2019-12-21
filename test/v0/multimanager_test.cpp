// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>
#include <metall_utility/open_mp.hpp>
#include "../test_utility.hpp"

namespace {
namespace omp = metall::utility::omp;

using chunk_no_type = uint32_t;
constexpr std::size_t k_chunk_size = 1UL << 21UL;

using manager_type = metall::v0::manager_v0<chunk_no_type, k_chunk_size>;

template <typename T>
using metall_allocator = typename manager_type::allocator_type<T>;

TEST(MultiManagerTest, SingleThread) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, metall_allocator<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<>, // Equal function
                                        boost::container::scoped_allocator_adaptor<metall_allocator<std::pair<const element_type,
                                                                                                              vector_type>>>>;

  const auto dir_path1(test_utility::make_test_dir_path(::testing::UnitTest::GetInstance()->current_test_info()->name()
                                                            + std::to_string(1)));
  const auto dir_path2(test_utility::make_test_dir_path(::testing::UnitTest::GetInstance()->current_test_info()->name()
                                                            + std::to_string(2)));

  {
    manager_type manager1(metall::create_only, dir_path1.c_str(), k_chunk_size * 8);
    manager_type manager2(metall::create_only, dir_path2.c_str(), k_chunk_size * 8);

    map_type *map1 = manager1.construct<map_type>("map")(manager1.get_allocator<>());
    map_type *map2 = manager2.construct<map_type>("map")(manager2.get_allocator<>());

    (*map1)[0].emplace_back(1);
    (*map1)[0].emplace_back(2);

    (*map2)[0].emplace_back(3);
    (*map2)[0].emplace_back(4);
  }

  {
    manager_type manager1(metall::open_only, dir_path1.c_str());
    manager_type manager2(metall::open_only, dir_path2.c_str());

    map_type *map1;
    std::size_t n1;
    std::tie(map1, n1) = manager1.find<map_type>("map");

    map_type *map2;
    std::size_t n2;
    std::tie(map2, n2) = manager2.find<map_type>("map");

    ASSERT_EQ((*map1)[0][0], 1);
    ASSERT_EQ((*map1)[0][1], 2);
    (*map1)[1].emplace_back(5);

    ASSERT_EQ((*map2)[0][0], 3);
    ASSERT_EQ((*map2)[0][1], 4);
    (*map2)[1].emplace_back(6);
  }

  {
    manager_type manager1(metall::open_only, dir_path1.c_str());
    manager_type manager2(metall::open_only, dir_path2.c_str());

    map_type *map1;
    std::size_t n1;
    std::tie(map1, n1) = manager1.find<map_type>("map");

    map_type *map2;
    std::size_t n2;
    std::tie(map2, n2) = manager2.find<map_type>("map");

    ASSERT_EQ((*map1)[0][0], 1);
    ASSERT_EQ((*map1)[0][1], 2);
    ASSERT_EQ((*map1)[1][0], 5);

    ASSERT_EQ((*map2)[0][0], 3);
    ASSERT_EQ((*map2)[0][1], 4);
    ASSERT_EQ((*map2)[1][0], 6);
  }
}

int get_num_threads() {
  int num_threads = 0;

  OMP_DIRECTIVE(parallel)
  {
    num_threads = omp::get_num_threads();
  }

  return num_threads;
}

TEST(MultiManagerTest, MultiThread) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, metall_allocator<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<>, // Equal function
                                        boost::container::scoped_allocator_adaptor<metall_allocator<std::pair<const element_type,
                                                                                                              vector_type>>>>;

  OMP_DIRECTIVE(parallel)
  {
    const auto dir_path(test_utility::make_test_dir_path(::testing::UnitTest::GetInstance()->current_test_info()->name()
                                                             + std::to_string(omp::get_thread_num())));

    manager_type manager(metall::create_only, dir_path.c_str(), k_chunk_size * 16);
    map_type *map = manager.construct<map_type>("map")(manager.get_allocator<>());

    for (int i = 0; i < 64; ++i) {
      (*map)[i % 8].emplace_back(i * omp::get_thread_num());
    }
  }

  for (int t = 0; t < get_num_threads(); ++t) {
    const auto dir_path(test_utility::make_test_dir_path(::testing::UnitTest::GetInstance()->current_test_info()->name()
                                                             + std::to_string(t)));
    manager_type manager(metall::open_only, dir_path.c_str());

    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    for (int i = 0; i < 64; ++i) {
      ASSERT_EQ((*map)[i % 8][i / 8], i * t);
    }
  }
}
}