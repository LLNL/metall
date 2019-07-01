// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include "gtest/gtest.h"

#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>
#include <metall/v0/kernel/object_size_manager.hpp>
#include "../test_utility.hpp"

namespace {
using namespace metall::detail;

using chunk_no_type = uint32_t;
static constexpr std::size_t k_chunk_size = 1 << 21;
using manager_type = metall::v0::manager_v0<chunk_no_type, k_chunk_size>;
template <typename T>
using allocator_type = typename manager_type::allocator_type<T>;

using object_size_mngr = metall::v0::kernel::object_size_manager<k_chunk_size, 1ULL << 48>;
constexpr std::size_t k_min_object_size = object_size_mngr::at(0);

const std::string& file_path() {
  const static std::string path(test_utility::test_file_path("ManagerTest"));
  return path;
}

TEST(ManagerTest, TinyAllocation) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size);

  const std::size_t alloc_size = k_min_object_size / 2;
  char *base_addr = nullptr;
  for (uint64_t i = 0; i < k_chunk_size / k_min_object_size; ++i) {
    auto addr = static_cast<char *>(manager.allocate(alloc_size));
    if (i == 0) {
      base_addr = addr;
    }
    ASSERT_EQ((addr - base_addr) % k_chunk_size, i * k_min_object_size);
  }

  for (uint64_t i = 0; i < k_chunk_size / k_min_object_size; ++i) {
    char *addr = base_addr + i * k_min_object_size;
    manager.deallocate(addr);
  }

  for (uint64_t i = 0; i < k_chunk_size / k_min_object_size; ++i) {
    auto addr = static_cast<char *>(manager.allocate(alloc_size));
    ASSERT_EQ((addr - base_addr) % k_chunk_size, i * k_min_object_size);
  }
}

TEST(ManagerTest, SmallAllocation) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size);

  const std::size_t alloc_size = k_min_object_size;
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

TEST(ManagerTest, MaxSmallAllocation) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size);

  const std::size_t alloc_size = object_size_mngr::at(object_size_mngr::num_small_sizes() - 1); // Max small object num_blocks

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

TEST(ManagerTest, MixedSmallAllocation) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 3);

  const std::size_t alloc_size1 = k_min_object_size;
  const std::size_t alloc_size2 = k_min_object_size * 2;
  const std::size_t alloc_size3 = object_size_mngr::at(object_size_mngr::num_small_sizes() - 1); // Max small object num_blocks

  char *base_addr = nullptr;
  for (uint64_t i = 0; i < k_chunk_size / alloc_size1; ++i) {
    auto addr1 = static_cast<char *>(manager.allocate(alloc_size1));
    if (i == 0) {
      base_addr = addr1;
    }
    ASSERT_EQ((addr1 - base_addr) % k_chunk_size, i * alloc_size1);

    if (i < k_chunk_size / alloc_size2) {
      auto addr2 = static_cast<char *>(manager.allocate(alloc_size2));
      ASSERT_EQ((addr2 - base_addr) % k_chunk_size, i * alloc_size2);
    }

    if (i < k_chunk_size / alloc_size3) {
      auto addr3 = static_cast<char *>(manager.allocate(alloc_size3));
      ASSERT_EQ((addr3 - base_addr) % k_chunk_size, i * alloc_size3);
    }
  }

  for (uint64_t i = 0; i < k_chunk_size / alloc_size1; ++i) {
    manager.deallocate(base_addr + i * alloc_size1);
    if (i < k_chunk_size / alloc_size2)
      manager.deallocate(base_addr + k_chunk_size + i * alloc_size2);
    if (i < k_chunk_size / alloc_size3)
      manager.deallocate(base_addr + 2 * k_chunk_size + i * alloc_size3);
  }

  for (uint64_t i = 0; i < k_chunk_size / alloc_size1; ++i) {
    auto addr1 = static_cast<char *>(manager.allocate(alloc_size1));
    ASSERT_EQ((addr1 - base_addr) % k_chunk_size, i * alloc_size1);

    if (i < k_chunk_size / alloc_size2) {
      auto addr2 = static_cast<char *>(manager.allocate(alloc_size2));
      ASSERT_EQ((addr2 - base_addr - k_chunk_size) % k_chunk_size, i * alloc_size2);
    }

    if (i < k_chunk_size / alloc_size3) {
      auto addr3 = static_cast<char *>(manager.allocate(alloc_size3));
      ASSERT_EQ((addr3 - base_addr - 2 * k_chunk_size) % k_chunk_size, i * alloc_size3);
    }
  }
}

TEST(ManagerTest, LargeAllocation) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 4);

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

TEST(ManagerTest, StlAllocator) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size);

  allocator_type<uint64_t> stl_allocator_instance(manager.get_allocator<uint64_t>());
  uint64_t *base_addr = nullptr;

  for (uint64_t i = 0; i < k_chunk_size / sizeof(uint64_t); ++i) {
    auto addr = stl_allocator_instance.allocate(1).get();
    if (i == 0) {
      base_addr = addr;
    }
    ASSERT_EQ((addr - base_addr) % k_chunk_size, i);
  }

  for (uint64_t i = 0; i < k_chunk_size / sizeof(uint64_t); ++i) {
    uint64_t *addr = base_addr + i;
    stl_allocator_instance.deallocate(addr, 1);
  }

  for (uint64_t i = 0; i < k_chunk_size / sizeof(uint64_t); ++i) {
    auto addr = stl_allocator_instance.allocate(1).get();
    ASSERT_EQ((addr - base_addr) % k_chunk_size, i);
  }
}

TEST(ManagerTest, Container) {
  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 8);
  using element_type = std::pair<uint64_t, uint64_t>;

  boost::interprocess::vector<element_type, allocator_type<element_type>> vector(manager.get_allocator<>());
  for (uint64_t i = 0; i < k_chunk_size / sizeof(element_type); ++i) {
    vector.emplace_back(element_type(i, i * 2));
  }
  for (uint64_t i = 0; i < k_chunk_size / sizeof(element_type); ++i) {
    ASSERT_EQ(vector[i], element_type(i, i * 2));
  }
}

TEST(ManagerTest, NestedContainer) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, typename manager_type::allocator_type<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<element_type>, // Equal function
                                        boost::container::scoped_allocator_adaptor<allocator_type<std::pair<const element_type, vector_type>>>>;

  manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 8);

  map_type map(manager.get_allocator<>());
  for (uint64_t i = 0; i < k_chunk_size / sizeof(element_type); ++i) {
    map[i % 8].push_back(i);
  }

  for (uint64_t i = 0; i < k_chunk_size / sizeof(element_type); ++i) {
    ASSERT_EQ(map[i % 8][i / 8], i);
  }
}

TEST(ManagerTest, PersistentConstructFind) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, typename manager_type::allocator_type<element_type>>;

  {
    manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 4);

    int *a = manager.construct<int>("int")(10);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.construct<vector_type>("vector_type")(manager.get_allocator<vector_type>());
    vec->emplace_back(10);
    vec->emplace_back(20);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());

    const auto ret1 = manager.find<int>("int");
    ASSERT_NE(ret1.first, nullptr);
    ASSERT_EQ(ret1.second, 1);
    int *a = ret1.first;
    ASSERT_EQ(*a, 10);

    const auto ret2 = manager.find<vector_type>("vector_type");
    ASSERT_NE(ret2.first, nullptr);
    ASSERT_EQ(ret2.second, 1);
    vector_type *vec = ret2.first;
    ASSERT_EQ(vec->at(0), 10);
    ASSERT_EQ(vec->at(1), 20);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());
    ASSERT_TRUE(manager.destroy<int>("int"));
    ASSERT_FALSE(manager.destroy<int>("int"));

    ASSERT_TRUE(manager.destroy<vector_type>("vector_type"));
    ASSERT_FALSE(manager.destroy<vector_type>("vector_type"));
  }
}

TEST(ManagerTest, PersistentConstructOrFind) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, typename manager_type::allocator_type<element_type>>;

  {
    manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 4);
    int *a = manager.find_or_construct<int>("int")(10);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.find_or_construct<vector_type>("vector_type")(manager.get_allocator<vector_type>());
    vec->emplace_back(10);
    vec->emplace_back(20);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());

    int *a = manager.find_or_construct<int>("int")(20);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.find_or_construct<vector_type>("vector_type")(manager.get_allocator<vector_type>());
    ASSERT_EQ(vec->at(0), 10);
    ASSERT_EQ(vec->at(1), 20);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());
    ASSERT_TRUE(manager.destroy<int>("int"));
    ASSERT_FALSE(manager.destroy<int>("int"));

    ASSERT_TRUE(manager.destroy<vector_type>("vector_type"));
    ASSERT_FALSE(manager.destroy<vector_type>("vector_type"));
  }
}

TEST(ManagerTest, PersistentNestedContainer) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, typename manager_type::allocator_type<element_type>>;
  using map_type = boost::unordered_map<element_type, // Key
                                        vector_type, // Value
                                        std::hash<element_type>, // Hash function
                                        std::equal_to<element_type>, // Equal function
                                        boost::container::scoped_allocator_adaptor<allocator_type<std::pair<const element_type, vector_type>>>>;

  {
    manager_type manager(metall::create_only, file_path().c_str(), k_chunk_size * 8);
    map_type *map = manager.construct<map_type>("map")(manager.get_allocator<>());
    (*map)[0].emplace_back(1);
    (*map)[0].emplace_back(2);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());
    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    ASSERT_EQ((*map)[0][0], 1);
    ASSERT_EQ((*map)[0][1], 2);
    (*map)[1].emplace_back(3);
  }

  {
    manager_type manager(metall::open_only, file_path().c_str());
    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    ASSERT_EQ((*map)[0][0], 1);
    ASSERT_EQ((*map)[0][1], 2);
    ASSERT_EQ((*map)[1][0], 3);
  }
}

TEST(ManagerTest, Sync) {
  using element_type = uint64_t;
  using vector_type = boost::interprocess::vector<element_type, typename manager_type::allocator_type<element_type>>;

  manager_type *manager;
  {
    manager = new manager_type(metall::create_only, file_path().c_str(), k_chunk_size * 4);

    int *a = manager->construct<int>("int")(10);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager->construct<vector_type>("vector_type")(manager->get_allocator<vector_type>());
    vec->resize(k_chunk_size * 2 / sizeof(element_type)); // Force to use multiple chunks
    for (uint64_t i = 0; i < vec->size(); ++i) {
      vec->at(i) = i;
    }
    manager->sync();
    // Do not free (destruct manage object) here on purpose
  }

  {
    manager_type manager2(metall::open_only, file_path().c_str());

    int *a;
    std::size_t n1;
    std::tie(a, n1) = manager2.find<int>("int");
    ASSERT_EQ(*a, 10);

    vector_type *vec;
    std::size_t n2;
    std::tie(vec, n2) = manager2.find<vector_type>("vector_type");

    ASSERT_EQ(vec->size(), k_chunk_size * 2 / sizeof(element_type));
    for (uint64_t i = 0; i < vec->size(); ++i) {
      ASSERT_EQ(vec->at(i), i);
    }
  }

  delete manager;
}

TEST(ManagerTest, AnonymousConstruct) {
  manager_type *manager;
  manager = new manager_type(metall::create_only, file_path().c_str(), k_chunk_size);

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
  manager = new manager_type(metall::create_only, file_path().c_str(), k_chunk_size);

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

}

