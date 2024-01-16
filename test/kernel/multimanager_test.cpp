// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>
#include <metall/utility/open_mp.hpp>
#include "../test_utility.hpp"

namespace {
namespace omp = metall::utility::omp;

using manager_type = metall::basic_manager<>;

template <typename T>
using metall_allocator = typename manager_type::allocator_type<T>;

TEST(MultiManagerTest, SingleThread) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, metall_allocator<element_type>>;

  test_utility::create_test_dir();
  const auto dir_path1(test_utility::make_test_path(std::to_string(1)));
  const auto dir_path2(test_utility::make_test_path(std::to_string(2)));

  {
    manager_type manager1(metall::create_only, dir_path1.c_str(),
                          manager_type::chunk_size() * 8);
    manager_type manager2(metall::create_only, dir_path2.c_str(),
                          manager_type::chunk_size() * 8);

    vector_type *vector1 =
        manager1.construct<vector_type>("vector")(manager1.get_allocator<>());
    vector_type *vector2 =
        manager2.construct<vector_type>("vector")(manager2.get_allocator<>());

    vector1->emplace_back(1);
    vector1->emplace_back(2);

    vector2->emplace_back(3);
    vector2->emplace_back(4);
  }

  {
    manager_type manager1(metall::open_only, dir_path1.c_str());
    manager_type manager2(metall::open_only, dir_path2.c_str());

    vector_type *vector1;
    std::size_t n1;
    std::tie(vector1, n1) = manager1.find<vector_type>("vector");

    vector_type *vector2;
    std::size_t n2;
    std::tie(vector2, n2) = manager2.find<vector_type>("vector");

    ASSERT_EQ(vector1->at(0), 1);
    ASSERT_EQ(vector1->at(1), 2);
    vector1->emplace_back(5);

    ASSERT_EQ(vector2->at(0), 3);
    ASSERT_EQ(vector2->at(1), 4);
    vector2->emplace_back(6);
  }

  {
    manager_type manager1(metall::open_only, dir_path1.c_str());
    manager_type manager2(metall::open_only, dir_path2.c_str());

    vector_type *vector1;
    std::size_t n1;
    std::tie(vector1, n1) = manager1.find<vector_type>("vector");

    vector_type *vector2;
    std::size_t n2;
    std::tie(vector2, n2) = manager2.find<vector_type>("vector");

    ASSERT_EQ(vector1->at(0), 1);
    ASSERT_EQ(vector1->at(1), 2);
    ASSERT_EQ(vector1->at(2), 5);

    ASSERT_EQ(vector2->at(0), 3);
    ASSERT_EQ(vector2->at(1), 4);
    ASSERT_EQ(vector2->at(2), 6);
  }
}

int get_num_threads() {
  int num_threads = 0;

  OMP_DIRECTIVE(parallel) { num_threads = omp::get_num_threads(); }

  return num_threads;
}

TEST(MultiManagerTest, MultiThread) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, metall_allocator<element_type>>;

  OMP_DIRECTIVE(parallel) {
    const auto dir_path(test_utility::make_test_path(
        "/" + std::to_string(omp::get_thread_num())));

    manager_type manager(metall::create_only, dir_path.c_str(),
                         manager_type::chunk_size() * 16);
    vector_type *vector =
        manager.construct<vector_type>("vector")(manager.get_allocator<>());

    for (int i = 0; i < 64; ++i) {
      vector->emplace_back(i * omp::get_thread_num());
    }
  }

  for (int t = 0; t < get_num_threads(); ++t) {
    const auto dir_path(test_utility::make_test_path("/" + std::to_string(t)));
    manager_type manager(metall::open_only, dir_path.c_str());

    vector_type *vector;
    std::size_t n;
    std::tie(vector, n) = manager.find<vector_type>("vector");

    for (int i = 0; i < 64; ++i) {
      ASSERT_EQ(vector->at(i), i * t);
    }
  }
}
}  // namespace