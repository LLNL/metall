// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <unordered_set>
#include <vector>
#include <utility>
#include <random>

#include <metall/metall.hpp>
#include <metall/kernel/bin_number_manager.hpp>
#include <metall/kernel/object_cache.hpp>
#include "../test_utility.hpp"

using bin_no_manager =
    metall::kernel::bin_number_manager<1ULL << 21, 1ULL << 40>;

// Dummy allocator for testing
// It keeps track of allocated objects
class dummy_allocator {
 public:
  explicit dummy_allocator(const std::size_t max_bin_no)
      : records(max_bin_no + 1), num_allocs(max_bin_no + 1, 0) {}

  void allocate(const bin_no_manager::bin_no_type bin_no, const std::size_t n,
                std::ptrdiff_t *const offsets) {
    for (std::size_t i = 0; i < n; ++i) {
      offsets[i] = (std::ptrdiff_t)(num_allocs[bin_no]++);
      records[bin_no].insert(offsets[i]);
    }
  }

  void deallocate(const bin_no_manager::bin_no_type bin_no, const std::size_t n,
                  const std::ptrdiff_t *const offsets) {
    for (std::size_t i = 0; i < n; ++i) {
      ASSERT_EQ(records[bin_no].count(offsets[i]), 1);
      records[bin_no].erase(offsets[i]);
    }
  }

  std::vector<std::unordered_set<std::ptrdiff_t>> records;
  std::vector<std::size_t> num_allocs;
};

using cache_type =
    metall::kernel::object_cache<std::size_t, std::ptrdiff_t, bin_no_manager,
                                 dummy_allocator>;
namespace {

TEST(ObjectCacheTest, Construct) {
  cache_type cache;
  ASSERT_GT(cache.max_per_cpu_cache_size(), 0);
  ASSERT_GT(cache.num_caches_per_cpu(), 0);
  ASSERT_GT(cache.max_bin_no(), 0);
}

TEST(ObjectCacheTest, Sequential) {
  cache_type cache;
  dummy_allocator alloc(cache.max_bin_no());

  for (int k = 0; k < 2; ++k) {
    std::vector<std::vector<std::ptrdiff_t>> offsets(cache.max_bin_no() + 1);
    for (std::size_t b = 0; b <= cache.max_bin_no(); ++b) {
      for (std::size_t i = 0; i < 256; ++i) {
        const auto off = cache.pop(b, &alloc, &dummy_allocator::allocate,
                                   &dummy_allocator::deallocate);
        offsets[b].push_back(off);
      }
    }

    for (std::size_t b = 0; b <= cache.max_bin_no(); ++b) {
      for (std::size_t i = 0; i < 256; ++i) {
        cache.push(b, offsets[b][i], &alloc, &dummy_allocator::deallocate);
      }
    }
  }

  // iterate over cache
  for (std::size_t c = 0; c < cache.num_caches(); ++c) {
    for (std::size_t b = 0; b <= cache.max_bin_no(); ++b) {
      for (auto it = cache.begin(c, b); it != cache.end(c, b); ++it) {
        const auto off = *it;
        ASSERT_EQ(alloc.records[b].count(off), 1);
      }
    }
  }

  // Deallocate all objects in cache
  cache.clear(&alloc, &dummy_allocator::deallocate);

  // alloc.records must be empty
  for (const auto &per_bin : alloc.records) {
    ASSERT_EQ(per_bin.size(), 0);
  }
}

TEST(ObjectCacheTest, SequentialSingleBinManyObjects) {
  cache_type cache;
  dummy_allocator alloc(cache.max_bin_no());

  std::vector<std::ptrdiff_t> offsets;
  for (std::size_t i = 0; i < 1 << 20; ++i) {
    const auto off = cache.pop(0, &alloc, &dummy_allocator::allocate,
                               &dummy_allocator::deallocate);
    offsets.push_back(off);
  }

  for (std::size_t i = 0; i < 1 << 20; ++i) {
    cache.push(0, offsets[i], &alloc, &dummy_allocator::deallocate);
  }

  // Deallocate all objects in cache
  cache.clear(&alloc, &dummy_allocator::deallocate);

  // alloc.records must be empty
  for (const auto &per_bin : alloc.records) {
    ASSERT_EQ(per_bin.size(), 0);
  }
}

TEST(ObjectCacheTest, Random) {
  cache_type cache;
  dummy_allocator alloc(cache.max_bin_no());

  std::vector<std::pair<bin_no_manager ::bin_no_type, std::ptrdiff_t>> offsets;

  for (std::size_t i = 0; i < 1 << 15; ++i) {
    const auto mode = std::rand() % 5;
    if (mode < 3) {
      const auto b = std::rand() % (cache.max_bin_no() + 1);
      offsets.emplace_back(b, cache.pop(b, &alloc, &dummy_allocator::allocate,
                                        &dummy_allocator::deallocate));
    } else {
      if (offsets.empty()) continue;
      const auto idx = std::rand() % offsets.size();
      cache.push(offsets[idx].first, offsets[idx].second, &alloc,
                 &dummy_allocator::deallocate);
      offsets.erase(offsets.begin() + idx);
    }
  }
  for (const auto &item : offsets) {
    cache.push(item.first, item.second, &alloc, &dummy_allocator::deallocate);
  }

  // Deallocate all objects in cache
  cache.clear(&alloc, &dummy_allocator::deallocate);

  // alloc.records must be empty
  for (std::size_t b = 0; b < alloc.records.size(); ++b) {
    ASSERT_EQ(alloc.records[b].size(), 0);
  }
}
}  // namespace