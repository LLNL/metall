// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <unordered_set>

#include <metall/metall.hpp>
#include <metall/kernel/object_size_manager.hpp>
#include "../test_utility.hpp"

namespace {
using namespace metall::detail;

using chunk_no_type = uint32_t;
static constexpr std::size_t k_chunk_size = 1 << 21;
using manager_type = metall::basic_manager<chunk_no_type, k_chunk_size>;
template <typename T>
using allocator_type = typename manager_type::allocator_type<T>;

using object_size_mngr = metall::kernel::object_size_manager<k_chunk_size, 1ULL << 48>;
constexpr std::size_t k_min_object_size = object_size_mngr::at(0);

const std::string &dir_path() {
  const static std::string path(test_utility::make_test_dir_path("ManagerTest"));
  return path;
}

TEST(ManagerTest, CreateAndOpenModes) {
  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      [[maybe_unused]] int *a = manager.construct<int>("int")(10);
    }
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      auto ret = manager.find<int>("int");
      ASSERT_EQ(ret.first, nullptr);
    }
  }

  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      [[maybe_unused]] int *a = manager.construct<int>("int")(10);
    }
    {
      manager_type manager(metall::open_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(*(static_cast<int *>(ret.first)), 10);
    }
  }

  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      [[maybe_unused]] int *a = manager.construct<int>("int")(10);
    }
    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(*(static_cast<int *>(ret.first)), 10);
    }
    {
      manager_type manager(metall::open_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(*(static_cast<int *>(ret.first)), 10);
    }
  }
}

TEST(ManagerTest, Consistency) {
  manager_type::remove(dir_path().c_str());

  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    // Must be inconsistent before closing
    ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));

    [[maybe_unused]] int *a = manager.construct<int>("dummy")(10);
  }
  ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));

  { // To make sure the consistent mark is cleared even after creating a new data store using an old dir path
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));

    [[maybe_unused]] int *a = manager.construct<int>("dummy")(10);
  }
  ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));

  {
    manager_type manager(metall::open_only, dir_path().c_str());
    ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));
  }
  ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));

  {
    manager_type manager(metall::open_read_only, dir_path().c_str());
    // Still consistent if it is opened with the read-only mode
    ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));
  }
  ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));
}

TEST(ManagerTest, TinyAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    const std::size_t alloc_size = k_min_object_size / 2;

    // To make sure that there is no duplicated allocation
    std::unordered_set<void *> set;

    for (uint64_t i = 0; i < k_chunk_size / k_min_object_size; ++i) {
      auto addr = static_cast<char *>(manager.allocate(alloc_size));
      ASSERT_EQ(set.count(addr), 0);
      set.insert(addr);
    }

    for (auto add : set) {
      manager.deallocate(add);
    }
  }
}

TEST(ManagerTest, SmallAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    const std::size_t alloc_size = k_min_object_size;

    // To make sure that there is no duplicated allocation
    std::unordered_set<void *> set;

    for (uint64_t i = 0; i < k_chunk_size / k_min_object_size; ++i) {
      auto addr = static_cast<char *>(manager.allocate(alloc_size));
      ASSERT_EQ(set.count(addr), 0);
      set.insert(addr);
    }

    for (auto add : set) {
      manager.deallocate(add);
    }
  }
}

TEST(ManagerTest, MaxSmallAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    // Max small allocation size
    const std::size_t alloc_size = object_size_mngr::at(object_size_mngr::num_small_sizes() - 1);

    // This test will fail if the ojbect cache is enabled to cache alloc_size
    char *base_addr = nullptr;
    for (uint64_t i = 0; i < k_chunk_size / alloc_size; ++i) {
      auto addr = static_cast<char *>(manager.allocate(alloc_size));
      if (i == 0) {
        base_addr = addr;
      }
      ASSERT_EQ((addr - base_addr) % k_chunk_size, i * alloc_size);
    }

    for (uint64_t i = 0; i < k_chunk_size / alloc_size; ++i) {
      char *addr = base_addr + i * alloc_size;
      manager.deallocate(addr);
    }

    for (uint64_t i = 0; i < k_chunk_size / alloc_size; ++i) {
      auto addr = static_cast<char *>(manager.allocate(alloc_size));
      ASSERT_EQ((addr - base_addr) % k_chunk_size, i * alloc_size);
    }
  }
}

TEST(ManagerTest, MixedSmallAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str());

    const std::size_t alloc_size1 = k_min_object_size * 2;
    const std::size_t alloc_size2 = k_min_object_size * 4;
    const std::size_t
        alloc_size3 = object_size_mngr::at(object_size_mngr::num_small_sizes() - 1); // Max small object num_blocks

    // To make sure that there is no duplicated allocation
    std::unordered_set<void *> set;

    for (uint64_t i = 0; i < k_chunk_size / alloc_size1; ++i) {
      {
        auto addr = static_cast<char *>(manager.allocate(alloc_size1));
        ASSERT_EQ(set.count(addr), 0);
        set.insert(addr);
      }
      {
        auto addr = static_cast<char *>(manager.allocate(alloc_size2));
        ASSERT_EQ(set.count(addr), 0);
        set.insert(addr);
      }
      {
        auto addr = static_cast<char *>(manager.allocate(alloc_size3));
        ASSERT_EQ(set.count(addr), 0);
        set.insert(addr);
      }
    }

    for (auto add : set) {
      manager.deallocate(add);
    }
  }
}

TEST(ManagerTest, LargeAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str());

    // Assume that the object cache is not used for large allocation
    char *base_addr = nullptr;
    {
      auto addr1 = static_cast<char *>(manager.allocate(k_chunk_size));
      base_addr = addr1;

      auto addr2 = static_cast<char *>(manager.allocate(k_chunk_size * 2));
      ASSERT_EQ((addr2 - base_addr), 1 * k_chunk_size);

      auto addr3 = static_cast<char *>(manager.allocate(k_chunk_size));
      ASSERT_EQ((addr3 - base_addr), 3 * k_chunk_size);

      manager.deallocate(base_addr);
      manager.deallocate(base_addr + k_chunk_size);
      manager.deallocate(base_addr + k_chunk_size * 3);
    }

    {
      auto addr1 = static_cast<char *>(manager.allocate(k_chunk_size));
      ASSERT_EQ((addr1 - base_addr), 0);

      auto addr2 = static_cast<char *>(manager.allocate(k_chunk_size * 2));
      ASSERT_EQ((addr2 - base_addr), 1 * k_chunk_size);

      auto addr3 = static_cast<char *>(manager.allocate(k_chunk_size));
      ASSERT_EQ((addr3 - base_addr), 3 * k_chunk_size);
    }
  }
}

TEST(ManagerTest, AlignedAllocation) {
  {
    manager_type manager(metall::create_only, dir_path().c_str());

    for (std::size_t alignment = k_min_object_size; alignment <= k_chunk_size; alignment *= 2) {
      for (std::size_t sz = alignment; sz <= k_chunk_size * 2; sz += alignment) {
        auto addr1 = static_cast<char *>(manager.allocate_aligned(sz, alignment));
        ASSERT_EQ(reinterpret_cast<uint64_t>(addr1) % alignment, 0);

        auto addr2 = static_cast<char *>(manager.allocate_aligned(sz, alignment));
        ASSERT_EQ(reinterpret_cast<uint64_t>(addr2) % alignment, 0);

        auto addr3 = static_cast<char *>(manager.allocate_aligned(sz, alignment));
        ASSERT_EQ(reinterpret_cast<uint64_t>(addr3) % alignment, 0);

        manager.deallocate(addr1);
        manager.deallocate(addr2);
        manager.deallocate(addr3);
      }

      {
        // Invalid argument
        // Alignment is not a power of 2
        auto addr1 = static_cast<char *>(manager.allocate_aligned(alignment + 1, alignment + 1));
        ASSERT_EQ(addr1, nullptr);

        // Invalid arguments
        // Size is not a multiple of alignment
        auto addr2 = static_cast<char *>(manager.allocate_aligned(alignment + 1, alignment));
        ASSERT_EQ(addr2, nullptr);
      }
    }

    {
      // Invalid argument
      // Alignment is smaller than k_min_object_size
      auto addr1 = static_cast<char *>(manager.allocate_aligned(8, 1));
      ASSERT_EQ(addr1, nullptr);

      // Invalid arguments
      // Alignment is larger than k_chunk_size
      auto addr2 = static_cast<char *>(manager.allocate_aligned(k_chunk_size * 2, k_chunk_size * 2));
      ASSERT_EQ(addr2, nullptr);
    }
  }
}

TEST(ManagerTest, Flush) {
  manager_type manager(metall::create_only, dir_path().c_str());

  [[maybe_unused]] auto a = manager.construct<int>("int")(10);

  manager.flush();

  ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));
}

TEST(ManagerTest, AnonymousConstruct) {
  manager_type *manager;
  manager = new manager_type(metall::create_only, dir_path().c_str(), 1UL << 30UL);

  int *const a = manager->construct<int>(metall::anonymous_instance)();
  ASSERT_NE(a, nullptr);

  // They have to be fail (return false values)
  const auto ret = manager->find<int>(metall::anonymous_instance);
  ASSERT_EQ(ret.first, nullptr);
  ASSERT_EQ(ret.second, 0);

  ASSERT_EQ(manager->destroy<int>(metall::anonymous_instance), false);

  manager->deallocate(a);

  delete manager;
}

TEST(ManagerTest, UniqueConstruct) {
  manager_type *manager;
  manager = new manager_type(metall::create_only, dir_path().c_str(), 1UL << 30UL);

  int *const a = manager->construct<int>(metall::unique_instance)();
  ASSERT_NE(a, nullptr);

  double *const b = manager->find_or_construct<double>(metall::unique_instance)();
  ASSERT_NE(b, nullptr);

  const auto ret_a = manager->find<int>(metall::unique_instance);
  ASSERT_EQ(ret_a.first, a);
  ASSERT_EQ(ret_a.second, 1);

  const auto ret_b = manager->find<double>(metall::unique_instance);
  ASSERT_EQ(ret_b.first, b);
  ASSERT_EQ(ret_b.second, 1);

  ASSERT_EQ(manager->destroy<int>(metall::unique_instance), true);
  ASSERT_EQ(manager->destroy<double>(metall::unique_instance), true);

  delete manager;
}

TEST(ManagerTest, UUID) {
  manager_type::remove(dir_path().c_str());
  std::string uuid;
  {
    manager_type manager(metall::create_only, dir_path().c_str());

    uuid = manager_type::get_uuid(dir_path().c_str());
    ASSERT_FALSE(uuid.empty());
  }

  { // Returns the same UUID?
    manager_type manager(metall::open_only, dir_path().c_str());
    ASSERT_EQ(manager_type::get_uuid(dir_path().c_str()), uuid);
  }

  { // Returns a new UUID?
    manager_type manager(metall::create_only, dir_path().c_str());
    ASSERT_NE(manager_type::get_uuid(dir_path().c_str()), uuid);
  }
}
}

