// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <stdlib.h>
#include <vector>
#include <array>
#include <list>
#include <numeric>
#include <cmath>
#include <random>
#include <algorithm>
#include <scoped_allocator>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>
#include <metall/utility/open_mp.hpp>
#include "../test_utility.hpp"

namespace {

namespace omp = metall::utility::omp;

// -------------------- //
// Manage Type
// -------------------- //
constexpr std::size_t k_min_object_size = 8;  // TODO: take from the file
using manager_type = metall::basic_manager<>;
template <typename T>
using allocator_type = typename manager_type::allocator_type<T>;

// -------------------- //
// TEST utility functions
// -------------------- //

/// \brief Check if there is no overlap among given allocation lists
template <typename addr_list_type>
void validate_overlap(const addr_list_type &addr_and_size_lists) {
  std::vector<std::pair<void *, void *>> allocation_range_list;
  for (const auto &addr_and_size : addr_and_size_lists) {
    void *begin_addr = addr_and_size.first;
    void *end_addr = static_cast<char *>(begin_addr) + addr_and_size.second;
    allocation_range_list.emplace_back(std::make_pair(begin_addr, end_addr));
  }

  std::sort(allocation_range_list.begin(), allocation_range_list.end(),
            [](const std::pair<void *, void *> lhd,
               const std::pair<void *, void *> rhd) -> bool {
              return lhd.first < rhd.first;
            });

  void *previous_end = allocation_range_list[0].first;
  for (const auto &begin_and_end : allocation_range_list) {
    ASSERT_LE(previous_end, begin_and_end.first);
    previous_end = begin_and_end.second;
  }
}

template <typename list_type>
std::pair<void *, void *> get_addr_range(const list_type &addr_and_size_lists) {
  void *begin = addr_and_size_lists[0].first;
  void *end = addr_and_size_lists[0].first;
  for (const auto &addr_and_size : addr_and_size_lists) {
    begin = std::min(begin, addr_and_size.first);
    end = std::max(
        end, static_cast<void *>(static_cast<char *>(addr_and_size.first) +
                                 addr_and_size.second));
  }

  return std::make_pair(begin, end);
}

template <typename list_type>
void shuffle_list(list_type *list) {
  std::random_device rd;
  std::mt19937_64 g(rd());
  std::shuffle(list->begin(), list->end(), g);
}

int get_num_threads() {
  int num_threads = 0;

  OMP_DIRECTIVE(parallel) { num_threads = omp::get_num_threads(); }

  return num_threads;
}

/// \brief
/// This validation fails if the total allocation size of any size is less than
/// manager_type::chunk_size()
template <typename list_type>
void run_alloc_dealloc_separated_test(const list_type &allocation_size_list) {
  // Allocate manager
  const auto dir(test_utility::make_test_path());
  manager_type manager(metall::create_only, dir);

  // Main loop
  std::pair<void *, void *> previous_allocation_rage(nullptr, nullptr);
  for (int k = 0; k < 2; ++k) {
    std::vector<std::pair<void *, std::size_t>> addr_and_size_array(
        allocation_size_list.size(), {nullptr, 0});

    // Allocation
    OMP_DIRECTIVE(parallel for)
    for (std::size_t i = 0; i < allocation_size_list.size(); ++i) {
      const std::size_t allocation_size = allocation_size_list[i];
      addr_and_size_array[i] =
          std::make_pair(manager.allocate(allocation_size), allocation_size);
    }

    // Validate allocated addresses
    // Check if there is no overlap
    validate_overlap(addr_and_size_array);

    // Deallocation
    OMP_DIRECTIVE(parallel for)
    for (std::size_t i = 0; i < addr_and_size_array.size(); ++i)
      manager.deallocate(addr_and_size_array[i].first);
  }
}

/// \brief
/// This validation fails if the total allocation size of any size is less than
/// manager_type::chunk_size()
template <typename list_type>
void run_alloc_dealloc_mixed_and_write_value_test(
    const list_type &allocation_size_list) {
  // Allocate manager
  const auto dir(test_utility::make_test_path());
  manager_type manager(metall::create_only, dir);

  // Main loop
  std::vector<std::pair<void *, std::size_t>> previous_addr_and_size_array(
      allocation_size_list.size(), {nullptr, 0});
  for (int k = 0; k < 2; ++k) {
    std::vector<std::pair<void *, std::size_t>> current_addr_and_size_array(
        allocation_size_list.size(), {nullptr, 0});

    OMP_DIRECTIVE(parallel for)
    for (std::size_t i = 0; i < allocation_size_list.size(); ++i) {
      const std::size_t allocation_size = allocation_size_list[i];
      void *const addr = manager.allocate(allocation_size);

      static_assert(sizeof(std::size_t) <= k_min_object_size,
                    "k_min_object_size must be equal to or large than "
                    "sizeof(std::size_t)");
      static_cast<std::size_t *>(addr)[0] =
          allocation_size;  // Write a value for validation
      current_addr_and_size_array[i] = std::make_pair(addr, allocation_size);

      if (k > 0) {
        manager.deallocate(previous_addr_and_size_array[i].first);
      }
    }

    // Make sure the latest allocated regions were not deallocated
    for (const auto &addr_and_size : current_addr_and_size_array) {
      ASSERT_EQ(static_cast<std::size_t *>(addr_and_size.first)[0],
                addr_and_size.second);
    }

    previous_addr_and_size_array = std::move(current_addr_and_size_array);
    shuffle_list(&previous_addr_and_size_array);
  }

  OMP_DIRECTIVE(parallel for)
  for (std::size_t i = 0; i < previous_addr_and_size_array.size(); ++i) {
    manager.deallocate(previous_addr_and_size_array[i].first);
  }
}

// -------------------- //
// TEST main functions
// -------------------- //
TEST(ManagerMultithreadsTest, CheckOpenMP) {
  EXPECT_TRUE(  // Expect Open MP is available
#ifdef _OPENMP
      true
#else
      false
#endif
  );

  EXPECT_GE(get_num_threads(), 2);  // Expect multi-threaded
}

TEST(ManagerMultithreadsTest, SmallAllocDeallocSeparated) {
  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 1);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 2);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, LargeAllocDeallocSeparated) {
  const std::size_t num_allocations_per_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size());
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size() * 2);
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size() * 4);
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size() * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}
#endif

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, SizeMixedAllocDeallocSeparated) {
  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 1);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), 1024, manager_type::chunk_size());
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              manager_type::chunk_size() * 2);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              manager_type::chunk_size() * 4);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              manager_type::chunk_size() * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}
#endif

TEST(ManagerMultithreadsTest, SmallAllocDeallocMixed) {
  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 2);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, LargeAllocDeallocMixed) {
  const std::size_t num_allocations_per_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size());
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size() * 2);
  allocation_size_list.insert(allocation_size_list.end(),
                              num_allocations_per_size, manager_type::chunk_size() * 4);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}
#endif

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, SizeMixedAllocDeallocMixed) {
  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              k_min_object_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), 1024, manager_type::chunk_size());
  allocation_size_list.insert(allocation_size_list.end(), 1024,
                              manager_type::chunk_size() * 4);
  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}
#endif

TEST(ManagerMultithreadsTest, ConstructAndFind) {
  using allocation_element_type = std::array<char, 256>;
  constexpr std::size_t num_allocates = 1024;

  const auto dir(test_utility::make_test_path());
  manager_type manager(metall::create_only, dir);

  std::vector<std::string> keys;
  for (std::size_t i = 0; i < num_allocates; ++i) {
    keys.emplace_back(std::to_string(i));
  }

  std::vector<std::vector<allocation_element_type *>> addr_list(
      get_num_threads());
  OMP_DIRECTIVE(parallel) {
    addr_list[omp::get_thread_num()].resize(keys.size(), nullptr);
  }

  // Concurrent find or construct test
  // (one of threads 'constructs' the object, and the rest 'find' the address)
  OMP_DIRECTIVE(parallel) {
    const int tid = omp::get_thread_num();
    for (std::size_t i = 0; i < keys.size(); ++i) {
      addr_list[tid][i] =
          manager.find_or_construct<allocation_element_type>(keys[i].c_str())();
    }
  }

  // All threads must point to the same address
  for (std::size_t t = 1; t < addr_list.size(); ++t) {
    for (std::size_t i = 0; i < addr_list[0].size(); ++i) {
      ASSERT_EQ(addr_list[0][i], addr_list[t][i]);
    }
  }

  int *num_deallocated = nullptr;
#if defined(__APPLE__)
  num_deallocated = static_cast<int *>(::malloc(keys.size() * sizeof(int)));
#else
  num_deallocated =
      static_cast<int *>(::aligned_alloc(4096, keys.size() * sizeof(int)));
#endif
  for (std::size_t i = 0; i < keys.size(); ++i) num_deallocated[i] = 0;

  OMP_DIRECTIVE(parallel) {
    for (std::size_t i = 0; i < keys.size(); ++i) {
      const bool ret =
          manager.destroy<allocation_element_type>(keys[i].c_str());
      if (ret) {
        OMP_DIRECTIVE(atomic)
        ++(num_deallocated[i]);
      }
    }
  }

  // There must be only one thread that destroys the object
  for (std::size_t i = 0; i < keys.size(); ++i) {
    ASSERT_EQ(num_deallocated[i], 1);
  }

  ::free(num_deallocated);
}
}  // namespace