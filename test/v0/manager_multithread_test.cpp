// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

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

#ifdef _OPENMP
#include <omp.h>
#endif

#include <metall/metall.hpp>
#include "../test_utility.hpp"

namespace {

/// -------------------------------------------------------------------------------- ///
/// Manage Type
/// -------------------------------------------------------------------------------- ///
using chunk_no_type = uint32_t;
static constexpr std::size_t k_chunk_size = 1ULL << 21;
constexpr std::size_t k_min_object_size = 8; // TODO: take from the file
using manager_type = metall::v0::manager_v0<chunk_no_type, k_chunk_size>;
template <typename T>
using allocator_type = typename manager_type::allocator_type<T>;


/// -------------------------------------------------------------------------------- ///
/// TEST utility functions
/// -------------------------------------------------------------------------------- ///

/// \brief Check there is no overlap among given allocation lists
/// \tparam check_no_blank If true, check if there is no blank between allocations
template <typename addr_list_type>
void validate_overlap(const addr_list_type &addr_and_size_lists,
                      const bool check_no_blank = false) {

  std::list<std::pair<void *, void *>> allocation_range_list;
  for (const auto &addr_and_size : addr_and_size_lists) {
    void *begin_addr = addr_and_size.first;
    void *end_addr = static_cast<char *>(begin_addr) + addr_and_size.second;
    allocation_range_list.emplace_back(std::make_pair(begin_addr, end_addr));
  }

  allocation_range_list.sort(
      [](const std::pair<void *, void *> lhd, const std::pair<void *, void *> rhd) -> bool {
        return lhd.first < rhd.first;
      });

  void *previous_end = allocation_range_list.front().first;
  for (const auto begin_and_end : allocation_range_list) {
    if (check_no_blank)
      ASSERT_EQ(previous_end, begin_and_end.first);
    else
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
    end = std::max(end, static_cast<void *>(static_cast<char *>(addr_and_size.first) + addr_and_size.second));
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

/// \brief
/// This validation fails if the total allocation size of any size is less than k_chunk_size
template <typename list_type>
void run_alloc_dealloc_separated_test(const list_type &allocation_size_list) {

  // Allocate manager
  const auto dir(test_utility::get_test_dir() + ::testing::UnitTest::GetInstance()->current_test_info()->name());
  manager_type manager(metall::create_only, dir.c_str());

  // Main loop
  std::pair<void *, void *> previous_allocation_rage(nullptr, nullptr);
  for (int k = 0; k < 2; ++k) {
    std::vector<std::pair<void *, std::size_t>> addr_and_size_array(allocation_size_list.size(),
                                                                    {nullptr, 0});

    // Allocation
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (std::size_t i = 0; i < allocation_size_list.size(); ++i) {
      const std::size_t allocation_size = allocation_size_list[i];
      addr_and_size_array[i] = std::make_pair(manager.allocate(allocation_size), allocation_size);
    }

    // Validate allocated addresses
    // Check if there is no overlap and blank
    validate_overlap(addr_and_size_array, true);

    // Deallocation
#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (std::size_t i = 0; i < addr_and_size_array.size(); ++i)
      manager.deallocate(addr_and_size_array[i].first);

    // Compare the begin and end addresses of the previous and current loop
    // to make sure all allocations were deallocated in the previous loop.
    // As we confirmed there is no blank in the range of allocations,
    // just checking the begin and end addresses is enough.
    const auto begin_end_addr = get_addr_range(addr_and_size_array);
    if (k == 0) {
      previous_allocation_rage = begin_end_addr;
    } else {
      ASSERT_EQ(begin_end_addr.first, previous_allocation_rage.first);
      ASSERT_EQ(begin_end_addr.second, previous_allocation_rage.second);
    }
  }
}

/// \brief
/// This validation fails if the total allocation size of any size is less than k_chunk_size
template <typename list_type>
void run_alloc_dealloc_mixed_and_write_value_test(const list_type &allocation_size_list) {

  // Allocate manager
  const auto dir(test_utility::get_test_dir() + ::testing::UnitTest::GetInstance()->current_test_info()->name());
  manager_type manager(metall::create_only, dir.c_str());

  // Main loop
  std::vector<std::pair<void *, std::size_t>> previous_addr_and_size_array(allocation_size_list.size(), {nullptr, 0});
  for (int k = 0; k < 2; ++k) {
    std::vector<std::pair<void *, std::size_t>> current_addr_and_size_array(allocation_size_list.size(), {nullptr, 0});

#ifdef _OPENMP
#pragma omp parallel for
#endif
    for (std::size_t i = 0; i < allocation_size_list.size(); ++i) {

      const std::size_t allocation_size = allocation_size_list[i];
      void *const addr = manager.allocate(allocation_size);

      static_assert(k_min_object_size <= sizeof(std::size_t),
                    "k_min_object_size must be equal to or large than sizeof(std::size_t)");
      static_cast<std::size_t *>(addr)[0] = allocation_size; // Write a value for validation
      current_addr_and_size_array[i] = std::make_pair(addr, allocation_size);

      if (k > 0) {
        manager.deallocate(previous_addr_and_size_array[i].first);
      }
    }

    // Make sure the latest allocated regions were not deallocated
    for (const auto &addr_and_size : current_addr_and_size_array) {
      ASSERT_EQ(static_cast<std::size_t *>(addr_and_size.first)[0], addr_and_size.second);
    }

    previous_addr_and_size_array = std::move(current_addr_and_size_array);
    shuffle_list(&previous_addr_and_size_array);
  }

#ifdef _OPENMP
#pragma omp parallel for
#endif
  for (std::size_t i = 0; i < previous_addr_and_size_array.size(); ++i) {
    manager.deallocate(previous_addr_and_size_array[i].first);
  }
}

/// -------------------------------------------------------------------------------- ///
/// TEST main functions
/// -------------------------------------------------------------------------------- ///
TEST(ManagerMultithreadsTest, CheckOpenMP) {
  EXPECT_TRUE(  // Expect Open MP is available
#ifdef _OPENMP
      true
#else
      false
#endif
             );

  EXPECT_GE(get_num_threads(), 2); // Expect multi-threaded
}

TEST(ManagerMultithreadsTest, SmallAllocDeallocSeparated) {

  const std::size_t num_allocations_per_size = k_chunk_size / k_min_object_size;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, LargeAllocDeallocSeparated) {

  const std::size_t num_allocations_per_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}
#endif

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, SizeMixedAllocDeallocSeparated) {

  const std::size_t num_allocations_per_small_size = k_chunk_size / k_min_object_size;
  const std::size_t num_allocations_per_large_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 8);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_separated_test(allocation_size_list);
}
#endif

TEST(ManagerMultithreadsTest, SmallAllocDeallocMixed) {

  const std::size_t num_allocations_per_size = k_chunk_size / k_min_object_size;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_min_object_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, LargeAllocDeallocMixed) {

  const std::size_t num_allocations_per_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_size, k_chunk_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}
#endif

#ifdef METALL_RUN_LARGE_SCALE_TEST
TEST(ManagerMultithreadsTest, SizeMixedAllocDeallocMixed) {

  const std::size_t num_allocations_per_small_size = k_chunk_size / k_min_object_size;
  const std::size_t num_allocations_per_large_size = 1024;

  std::vector<std::size_t> allocation_size_list;
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_small_size, k_min_object_size * 8);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 2);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 4);
  allocation_size_list.insert(allocation_size_list.end(), num_allocations_per_large_size, k_chunk_size * 8);

  shuffle_list(&allocation_size_list);

  run_alloc_dealloc_mixed_and_write_value_test(allocation_size_list);
}
#endif

TEST(ManagerMultithreadsTest, ConstructAndFind) {
  using allocation_element_type = std::array<char, 256>;

  const std::size_t file_size = k_chunk_size;
  const auto dir(test_utility::get_test_dir() + ::testing::UnitTest::GetInstance()->current_test_info()->name());
  manager_type manager(metall::create_only, dir.c_str());

  for (uint64_t i = 0; i < file_size / sizeof(allocation_element_type); ++i) {

    // Allocation (one of threads 'construct' the object and the rest 'find' the address)
    std::vector<allocation_element_type *> addr_list(get_num_threads(), nullptr);
#ifdef _OPENMP
#pragma omp parallel
#endif
    {
      addr_list[get_thread_num()] = manager.find_or_construct<allocation_element_type>(std::to_string(i).c_str())();
    }

    // All threads must point to the same address
    for (int t = 0; t < get_num_threads() - 1; ++t) {
      ASSERT_EQ(addr_list[t], addr_list[t + 1]);
    }

    // Deallocation
    int num_succeeded = 0;
#ifdef _OPENMP
#pragma omp parallel
#endif
    {
      const bool ret = manager.destroy<allocation_element_type>(std::to_string(i).c_str());
#ifdef _OPENMP
#pragma omp critical
#endif
      {
        num_succeeded += ret;
      }
    }
    ASSERT_EQ(num_succeeded, 1);
  }
}

// TODO: test concurrent find_or_construct and destroy
}