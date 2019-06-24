// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#ifdef _OPENMP
#include <omp.h>
#endif

#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>

namespace {
using chunk_no_type = uint32_t;
constexpr std::size_t k_chunk_size = 1UL << 21UL;

using manager_type = metall::v0::manager_v0<chunk_no_type, k_chunk_size>;

template <typename T>
using metall_allocator = typename manager_type::allocator_type<T>;

TEST(ManagerTest, SingleThread) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, metall_allocator<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<>, // Equal function
                                        boost::container::scoped_allocator_adaptor<metall_allocator<std::pair<const element_type,
                                                                                                              vector_type>>>>;
  {
    manager_type manager1(metall::create_only, "/tmp/manager_test_file1", k_chunk_size * 8);
    manager_type manager2(metall::create_only, "/tmp/manager_test_file2", k_chunk_size * 8);

    map_type *map1 = manager1.construct<map_type>("map")(manager1.get_allocator<>());
    map_type *map2 = manager2.construct<map_type>("map")(manager2.get_allocator<>());

    (*map1)[0].emplace_back(1);
    (*map1)[0].emplace_back(2);

    (*map2)[0].emplace_back(3);
    (*map2)[0].emplace_back(4);
  }

  {
    manager_type manager1(metall::open_only, "/tmp/manager_test_file1");
    manager_type manager2(metall::open_only, "/tmp/manager_test_file2");

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
    manager_type manager1(metall::open_only, "/tmp/manager_test_file1");
    manager_type manager2(metall::open_only, "/tmp/manager_test_file2");

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
  int num_threads = 1;

#ifdef _OPENMP
#pragma omp parallel
  if (::omp_get_thread_num() == 0)
    num_threads = ::omp_get_num_threads();
#endif

  return num_threads;
}

int get_thread_num() {
#ifdef _OPENMP
  return ::omp_get_thread_num();
#else
  return 0;
#endif

}

TEST(ManagerTest, MultiThread) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, metall_allocator<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<>, // Equal function
                                        boost::container::scoped_allocator_adaptor<metall_allocator<std::pair<const element_type,
                                                                                                              vector_type>>>>;

#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    const std::string file = "/tmp/manager_test_file" + std::to_string(get_thread_num());
    manager_type manager(metall::create_only, file.c_str(), k_chunk_size * 16);
    map_type *map = manager.construct<map_type>("map")(manager.get_allocator<>());

    for (int i = 0; i < 64; ++i) {
      (*map)[i % 8].emplace_back(i * get_thread_num());
    }
  }

  for (int t = 0; t < get_num_threads(); ++t) {
    const std::string file = "/tmp/manager_test_file" + std::to_string(t);
    manager_type manager(metall::open_only, file.c_str());

    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    for (int i = 0; i < 64; ++i) {
      ASSERT_EQ((*map)[i % 8][i / 8], i * t);
    }
  }
}
}