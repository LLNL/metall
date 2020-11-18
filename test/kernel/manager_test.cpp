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
  const static std::string path(test_utility::make_test_path());
  return path;
}

TEST(ManagerTest, CreateAndOpenModes) {
  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.construct<int>("int")(10);
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      auto ret = manager.find<int>("int");
      ASSERT_EQ(ret.first, nullptr);
      ASSERT_FALSE(manager.destroy<int>("int"));
    }
  }

  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.construct<int>("int")(10);
    }
    {
      manager_type manager(metall::open_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(*(static_cast<int *>(ret.first)), 10);
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }

  {
    manager_type::remove(dir_path().c_str());
    {
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.construct<int>("int")(10);
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
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, ConstructArray) {
  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.construct<int>("int")[2](10);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(ret.second, 2);
      auto a = ret.first;
      ASSERT_EQ(a[0], 10);
      ASSERT_EQ(a[1], 10);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, findOrConstruct) {
  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.find_or_construct<int>("int")(10);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      int *a = manager.find_or_construct<int>("int")(20);
      ASSERT_EQ(*a, 10);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, findOrConstructArray) {
  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.find_or_construct<int>("int")[2](10);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      int *a = manager.find_or_construct<int>("int")[2](20);
      ASSERT_EQ(a[0], 10);
      ASSERT_EQ(a[1], 10);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, ConstructContainers) {
  {
    using vec_t = std::vector<int, metall::manager::allocator_type<int>>;
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      manager.construct<vec_t>("vecs")[2](2, 10, manager.get_allocator<int>());
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      auto ret = manager.find<vec_t>("vecs");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(ret.second, 2);
      vec_t *a = ret.first;
      ASSERT_EQ(a[0].size(), 2);
      ASSERT_EQ(a[1].size(), 2);
      ASSERT_EQ(a[0][0], 10);
      ASSERT_EQ(a[0][1], 10);
      ASSERT_EQ(a[1][0], 10);
      ASSERT_EQ(a[1][1], 10);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<vec_t>("vecs"));
    }
  }
}

TEST(ManagerTest, ConstructWithIterator) {
  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      int values[2] = {10, 20};
      manager.construct_it<int>("int")[2](&values[0]);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      auto ret = manager.find<int>("int");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(ret.second, 2);
      auto a = ret.first;
      ASSERT_EQ(a[0], 10);
      ASSERT_EQ(a[1], 20);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, ConstructObjectsWithIterator) {
  struct data {
    int a;
    float b;
    data(int _a, float _b)
        : a(_a), b(_b) {}
  };

  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      int values1[2] = {10, 20};
      float values2[2] = {0.1, 0.2};
      manager.construct_it<data>("data")[2](&values1[0], &values2[0]);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      auto ret = manager.find<data>("data");
      ASSERT_NE(ret.first, nullptr);
      ASSERT_EQ(ret.second, 2);
      data *d = ret.first;
      ASSERT_EQ(d[0].a, 10);
      ASSERT_EQ(d[0].b, (float)0.1);
      ASSERT_EQ(d[1].a, 20);
      ASSERT_EQ(d[1].b, (float)0.2);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<data>("data"));
    }
  }
}

TEST(ManagerTest, FindOrConstructWithIterator) {
  {
    {
      manager_type::remove(dir_path().c_str());
      manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
      int values[2] = {10, 20};
      manager.find_or_construct_it<int>("int")[2](&values[0]);
    }

    {
      manager_type manager(metall::open_read_only, dir_path().c_str());
      int values[2] = {30, 40};
      int *a = manager.find_or_construct_it<int>("int")[2](&values[0]);
      ASSERT_EQ(a[0], 10);
      ASSERT_EQ(a[1], 20);
    }

    {
      manager_type manager(metall::open_only, dir_path().c_str());
      ASSERT_TRUE(manager.destroy<int>("int"));
    }
  }
}

TEST(ManagerTest, Destroy) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_FALSE(manager.destroy<int>("named_obj"));
    ASSERT_FALSE(manager.destroy<int>(metall::unique_instance));
    ASSERT_FALSE(manager.destroy<int>("array_obj"));

    manager.construct<int>("named_obj")();
    manager.construct<int>(metall::unique_instance)();
    manager.construct<int>("array_obj")[2](10);

    ASSERT_TRUE(manager.destroy<int>("named_obj"));
    ASSERT_FALSE(manager.destroy<int>("named_obj"));

    ASSERT_TRUE(manager.destroy<int>(metall::unique_instance));
    ASSERT_FALSE(manager.destroy<int>(metall::unique_instance));

    ASSERT_TRUE(manager.destroy<int>("array_obj"));
    ASSERT_FALSE(manager.destroy<int>("array_obj"));
  }

  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    manager.construct<int>("named_obj")();
    manager.construct<int>(metall::unique_instance)();
    manager.construct<int>("array_obj")[2](10);
  }

  // Destroy after restoring
  {
    manager_type manager(metall::open_only, dir_path().c_str());

    ASSERT_TRUE(manager.destroy<int>("named_obj"));
    ASSERT_TRUE(manager.destroy<int>(metall::unique_instance));
    ASSERT_TRUE(manager.destroy<int>("array_obj"));
  }
}

TEST(ManagerTest, DestroyPtr) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    int *named_obj = manager.construct<int>("named_obj")();
    int *unique_obj = manager.construct<int>(metall::unique_instance)();
    int *anonymous_obj = manager.construct<int>(metall::anonymous_instance)();
    int *array_obj = manager.construct<int>("array_obj")[2](10);

    ASSERT_TRUE(manager.destroy_ptr(named_obj));
    ASSERT_FALSE(manager.destroy_ptr(named_obj));

    ASSERT_TRUE(manager.destroy_ptr(unique_obj));
    ASSERT_FALSE(manager.destroy_ptr(unique_obj));

    ASSERT_TRUE(manager.destroy_ptr(anonymous_obj));
    ASSERT_FALSE(manager.destroy_ptr(anonymous_obj));

    ASSERT_TRUE(manager.destroy_ptr(array_obj));
    ASSERT_FALSE(manager.destroy_ptr(array_obj));
  }

  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    manager.construct<int>("named_obj")();
    manager.construct<int>(metall::unique_instance)();
    manager.construct<int>("array_obj")[2](10);
    int *anonymous_obj = manager.construct<int>(metall::anonymous_instance)();
    manager.construct<metall::offset_ptr<int>>("metall::offset_ptr<int>")(anonymous_obj);
  }

  // Destroy after restoring
  {
    manager_type manager(metall::open_only, dir_path().c_str());

    ASSERT_TRUE(manager.destroy_ptr(manager.find<int>("named_obj").first));
    ASSERT_TRUE(manager.destroy_ptr(manager.find<int>(metall::unique_instance).first));
    ASSERT_TRUE(manager.destroy_ptr(manager.find<int>("array_obj").first));
    metall::offset_ptr<int> *ptr = manager.find<metall::offset_ptr<int>>("metall::offset_ptr<int>").first;
    ASSERT_TRUE(manager.destroy_ptr(ptr->get())); // destroy anonymous object
    ASSERT_TRUE(manager.destroy_ptr(ptr));
  }
}

TEST(ManagerTest, DestroyDestruct) {

  struct data {
    int *count;
    ~data() {
      --(*count);
    }
  };

  // -- Check if destructors are called in destroy() -- //
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    int count = 3;
    auto *data_obj = manager.construct<data>("named_obj")();
    data_obj->count = &count;

    auto *data_array = manager.construct<data>("array_obj")[2]();
    data_array[0].count = &count;
    data_array[1].count = &count;

    ASSERT_EQ(count, 3);
    manager.destroy<data>("named_obj");
    ASSERT_EQ(count, 2);
    manager.destroy<data>("array_obj");
    ASSERT_EQ(count, 0);
  }
}

TEST(ManagerTest, GetInstanceName) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_STREQ(manager.get_instance_name(manager.construct<int>("named_obj")()), "named_obj");
    ASSERT_STREQ(manager.get_instance_name(manager.construct<int>(metall::unique_instance)()), typeid(int).name());
    ASSERT_STREQ(manager.get_instance_name(manager.construct<int>(metall::anonymous_instance)()), (const char *)0);

    manager.construct<metall::offset_ptr<int>>("ptr<int>")(manager.construct<int>(metall::anonymous_instance)());
  }

  {
    manager_type manager(metall::open_read_only, dir_path().c_str());
    ASSERT_STREQ(manager.get_instance_name(manager.find<int>("named_obj").first), "named_obj");
    ASSERT_STREQ(manager.get_instance_name(manager.find<int>(metall::unique_instance).first), typeid(int).name());

    metall::offset_ptr<int> *ptr = manager.find<metall::offset_ptr<int>>("ptr<int>").first;
    int *anonymous_obj = ptr->get();
    ASSERT_STREQ(manager.get_instance_name(anonymous_obj), (const char *)0);
  }
}

TEST(ManagerTest, GetInstanceType) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_EQ(manager.get_instance_type(manager.construct<int>("named_obj")()),
              metall::manager::instance_type::named_type);
    ASSERT_EQ(manager.get_instance_type(manager.construct<int>(metall::unique_instance)()),
              metall::manager::instance_type::unique_type);
    ASSERT_EQ(manager.get_instance_type(manager.construct<int>(metall::anonymous_instance)()),
              metall::manager::instance_type::anonymous_type);

    manager.construct<metall::offset_ptr<int>>("ptr<int>")(manager.construct<int>(metall::anonymous_instance)());
  }

  {
    manager_type manager(metall::open_read_only, dir_path().c_str());

    ASSERT_EQ(manager.get_instance_type(manager.find<int>("named_obj").first),
              metall::manager::instance_type::named_type);
    ASSERT_EQ(manager.get_instance_type(manager.find<int>(metall::unique_instance).first),
              metall::manager::instance_type::unique_type);

    metall::offset_ptr<int> *ptr = manager.find<metall::offset_ptr<int>>("ptr<int>").first;
    int *anonymous_obj = ptr->get();
    ASSERT_EQ(manager.get_instance_type(anonymous_obj), metall::manager::instance_type::anonymous_type);
  }
}

TEST(ManagerTest, GetInstanceLength) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_EQ(manager.get_instance_length(manager.construct<int>("named_obj")()), 1);
    ASSERT_EQ(manager.get_instance_length(manager.construct<int>(metall::unique_instance)()), 1);
    ASSERT_EQ(manager.get_instance_length(manager.construct<int>(metall::anonymous_instance)()), 1);
    manager.construct<metall::offset_ptr<int>>("ptr<int>")(manager.construct<int>(metall::anonymous_instance)());

    // Change data type to avoid duplicate unique instances and anonymous instances
    ASSERT_EQ(manager.get_instance_length(manager.construct<float>("array_obj")[2]()), 2);
    ASSERT_EQ(manager.get_instance_length(manager.construct<float>(metall::unique_instance)[2]()), 2);
    ASSERT_EQ(manager.get_instance_length(manager.construct<float>(metall::anonymous_instance)[2]()), 2);
    manager.construct<metall::offset_ptr<float>>("ptr<float>")(manager.construct<float>(metall::anonymous_instance)[2]());
  }

  {
    manager_type manager(metall::open_read_only, dir_path().c_str());

    {
      ASSERT_EQ(manager.get_instance_length(manager.find<int>("named_obj").first), 1);
      ASSERT_EQ(manager.get_instance_length(manager.find<int>(metall::unique_instance).first), 1);
      metall::offset_ptr<int> *ptr = manager.find<metall::offset_ptr<int>>("ptr<int>").first;
      int *anonymous_obj = ptr->get();
      ASSERT_EQ(manager.get_instance_length(anonymous_obj), 1);
    }

    {
      ASSERT_EQ(manager.get_instance_length(manager.find<float>("array_obj").first), 2);
      ASSERT_EQ(manager.get_instance_length(manager.find<float>(metall::unique_instance).first), 2);
      metall::offset_ptr<float> *ptr = manager.find<metall::offset_ptr<float>>("ptr<float>").first;
      float *anonymous_obj = ptr->get();
      ASSERT_EQ(manager.get_instance_length(anonymous_obj), 2);
    }
  }
}

TEST(ManagerTest, CountObjects) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_EQ(manager.get_num_named_objects(), 0);
    manager.construct<int>("named_obj1")();
    ASSERT_EQ(manager.get_num_named_objects(), 1);
    manager.construct<float>("named_obj2")();
    ASSERT_EQ(manager.get_num_named_objects(), 2);

    ASSERT_EQ(manager.get_num_unique_objects(), 0);
    manager.construct<int>(metall::unique_instance)();
    ASSERT_EQ(manager.get_num_unique_objects(), 1);
    manager.construct<float>(metall::unique_instance)();
    ASSERT_EQ(manager.get_num_unique_objects(), 2);

    ASSERT_TRUE(manager.destroy<int>("named_obj1"));
    ASSERT_EQ(manager.get_num_named_objects(), 1);
    ASSERT_TRUE(manager.destroy<float>("named_obj2"));
    ASSERT_EQ(manager.get_num_named_objects(), 0);

    ASSERT_TRUE(manager.destroy<int>(metall::unique_instance));
    ASSERT_EQ(manager.get_num_unique_objects(), 1);
    ASSERT_TRUE(manager.destroy<float>(metall::unique_instance));
    ASSERT_EQ(manager.get_num_unique_objects(), 0);
  }

  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    manager.construct<int>("named_obj1")();
    manager.construct<int>(metall::unique_instance)();
    manager.construct<float>("named_obj2")();
    manager.construct<float>(metall::unique_instance)();
  }

  {
    manager_type manager(metall::open_only, dir_path().c_str());

    ASSERT_EQ(manager.get_num_named_objects(), 2);
    ASSERT_TRUE(manager.destroy<int>("named_obj1"));
    ASSERT_EQ(manager.get_num_named_objects(), 1);
    ASSERT_TRUE(manager.destroy<float>("named_obj2"));
    ASSERT_EQ(manager.get_num_named_objects(), 0);

    ASSERT_EQ(manager.get_num_unique_objects(), 2);
    ASSERT_TRUE(manager.destroy<int>(metall::unique_instance));
    ASSERT_EQ(manager.get_num_unique_objects(), 1);
    ASSERT_TRUE(manager.destroy<float>(metall::unique_instance));
    ASSERT_EQ(manager.get_num_unique_objects(), 0);
  }
}

TEST(ManagerTest, NamedObjectIterator) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_EQ(manager.named_begin(), manager.named_end());
    ASSERT_EQ(manager.unique_begin(), manager.unique_end());

    manager.construct<int>("named_obj1")();
    ASSERT_STREQ(manager.named_begin()->name.c_str(), "named_obj1");

    manager.construct<float>("named_obj2")();
    ASSERT_EQ(manager.unique_begin(), manager.unique_end());

    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = manager.named_begin(); itr != manager.named_end(); ++itr) {
      found1 |= (itr->name == "named_obj1");
      found2 |= (itr->name == "named_obj2");
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);

    ASSERT_TRUE(manager.destroy<int>("named_obj1"));
    ASSERT_STREQ(manager.named_begin()->name.c_str(), "named_obj2");

    ASSERT_TRUE(manager.destroy<float>("named_obj2"));
    ASSERT_EQ(manager.named_begin(), manager.named_end());
    ASSERT_EQ(manager.unique_begin(), manager.unique_end());
  }
}

TEST(ManagerTest, UniqueObjectIterator) {
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_EQ(manager.named_begin(), manager.named_end());
    ASSERT_EQ(manager.unique_begin(), manager.unique_end());

    manager.construct<int>(metall::unique_instance)();
    ASSERT_STREQ(manager.unique_begin()->name.c_str(), typeid(int).name());

    manager.construct<float>(metall::unique_instance)();
    ASSERT_EQ(manager.named_begin(), manager.named_end());

    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = manager.unique_begin(); itr != manager.unique_end(); ++itr) {
      found1 |= (itr->name == typeid(int).name());
      found2 |= (itr->name == typeid(float).name());
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);

    ASSERT_TRUE(manager.destroy<int>(metall::unique_instance));
    ASSERT_STREQ(manager.unique_begin()->name.c_str(), typeid(float).name());

    ASSERT_TRUE(manager.destroy<float>(metall::unique_instance));
    ASSERT_EQ(manager.named_begin(), manager.named_end());
    ASSERT_EQ(manager.unique_begin(), manager.unique_end());
  }
}

TEST(ManagerTest, GetSegment) {
  manager_type::remove(dir_path().c_str());
  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);
    auto* obj = manager.construct<int>(metall::unique_instance)();
    ASSERT_EQ(manager.unique_begin()->offset + static_cast<const char *>(manager.get_kernel()->get_segment()),
              reinterpret_cast<char *>(obj));
  }
}

TEST(ManagerTest, Consistency) {
  manager_type::remove(dir_path().c_str());

  {
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    // Must be inconsistent before closing
    ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));

    manager.construct<int>("dummy")(10);
  }
  ASSERT_TRUE(manager_type::consistent(dir_path().c_str()));

  { // To make sure the consistent mark is cleared even after creating a new data store using an old dir path
    manager_type manager(metall::create_only, dir_path().c_str(), 1UL << 30UL);

    ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));

    manager.construct<int>("dummy")(10);
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
    manager_type::remove(dir_path().c_str());
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
    manager_type::remove(dir_path().c_str());
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
    manager_type::remove(dir_path().c_str());
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
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str());

    const std::size_t alloc_size1 = k_min_object_size * 2;
    const std::size_t alloc_size2 = k_min_object_size * 4;
    const std::size_t
        alloc_size3 = object_size_mngr::at(object_size_mngr::num_small_sizes() - 1); // Max small object num_blocks

    // To make sure that there is no duplicated allocation
    std::unordered_set<void *> set;

    for (uint64_t i = 0; i < (k_chunk_size / alloc_size1) * 4; ++i) {
      {
        auto addr = static_cast<char *>(manager.allocate(alloc_size1));
        ASSERT_EQ(set.count(addr), 0);
        set.insert(addr);
      }

      if (i < (k_chunk_size / alloc_size2) * 4) {
        auto addr = static_cast<char *>(manager.allocate(alloc_size2));
        ASSERT_EQ(set.count(addr), 0);
        set.insert(addr);
      }

      if (i < (k_chunk_size / alloc_size3) * 4) {
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
    manager_type::remove(dir_path().c_str());
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
    manager_type::remove(dir_path().c_str());
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
  manager_type::remove(dir_path().c_str());
  manager_type manager(metall::create_only, dir_path().c_str());

  manager.construct<int>("int")(10);

  manager.flush();

  ASSERT_FALSE(manager_type::consistent(dir_path().c_str()));
}

TEST(ManagerTest, AnonymousConstruct) {
  manager_type::remove(dir_path().c_str());
  manager_type *manager;
  manager = new manager_type(metall::create_only, dir_path().c_str(), 1UL << 30UL);

  int *const a = manager->construct<int>(metall::anonymous_instance)();
  ASSERT_NE(a, nullptr);

  // They have to be fail (return false values)
  const auto ret = manager->find<int>(metall::anonymous_instance);
  ASSERT_EQ(ret.first, nullptr);
  ASSERT_EQ(ret.second, 0);

  manager->deallocate(a);

  delete manager;
}

TEST(ManagerTest, UniqueConstruct) {
  manager_type::remove(dir_path().c_str());
  manager_type *manager;
  manager = new manager_type(metall::create_only, dir_path().c_str(), 1UL << 30UL);

  int *const a = manager->construct<int>(metall::unique_instance)();
  ASSERT_NE(a, nullptr);

  double *const b = manager->find_or_construct<double>(metall::unique_instance)();
  ASSERT_NE(b, nullptr);

  ASSERT_EQ(manager->find<int>(metall::unique_instance).first, a);
  ASSERT_EQ(manager->find<int>(metall::unique_instance).second, 1);

  ASSERT_EQ(manager->find<double>(metall::unique_instance).first, b);
  ASSERT_EQ(manager->find<double>(metall::unique_instance).second, 1);

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

TEST(ManagerTest, Version) {
  manager_type::remove(dir_path().c_str());
  {
    manager_type manager(metall::create_only, dir_path().c_str());
    ASSERT_EQ(manager_type::get_version(dir_path().c_str()), METALL_VERSION);
  }

  {
    manager_type manager(metall::open_only, dir_path().c_str());
    ASSERT_EQ(manager_type::get_version(dir_path().c_str()), METALL_VERSION);
  }
}

TEST(ManagerTest, Description) {

  // Set and get with non-static method
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str());

    ASSERT_TRUE(manager.set_description("description1"));
    std::string description;
    ASSERT_TRUE(manager.get_description(&description));
    ASSERT_STREQ(description.c_str(), "description1");
  }

  // Get with static method
  {
    std::string description;
    ASSERT_TRUE(manager_type::get_description(dir_path().c_str(), &description));
    ASSERT_STREQ(description.c_str(), "description1");
  }

  // Set with static method
  {
    manager_type::remove(dir_path().c_str());
    manager_type manager(metall::create_only, dir_path().c_str()); // Make a new data store
    ASSERT_TRUE(manager_type::set_description(dir_path().c_str(), "description2"));
  }

  // Get with non-static method
  {
    manager_type manager(metall::open_only, dir_path().c_str());
    std::string description;
    ASSERT_TRUE(manager.get_description(&description));
    ASSERT_STREQ(description.c_str(), "description2");
  }
}
}

