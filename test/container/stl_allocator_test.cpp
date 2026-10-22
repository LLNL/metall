// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <memory>
#include <unordered_set>
#include <filesystem>
#include <cstdint>
#include <type_traits>
#include <utility>

#include <boost/container/scoped_allocator.hpp>
#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>
#include <metall/metall.hpp>
#include <metall/container/vector.hpp>
#include "../test_utility.hpp"

namespace {
namespace fs = std::filesystem;

template <typename T>
using alloc_type = metall::manager::allocator_type<T>;

const fs::path &dir_path() {
  const static fs::path path(test_utility::make_test_path());
  return path;
}

TEST(StlAllocatorTest, Types) {
  {
    using T = std::byte;
    using alloc_t = alloc_type<T>;

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::allocator_type),
                    typeid(alloc_t));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::value_type),
                    typeid(T));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::pointer),
                    typeid(alloc_t::pointer));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::pointer),
                    typeid(metall::offset_ptr<T>));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::const_pointer),
                    typeid(alloc_t::const_pointer));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::const_pointer),
                    typeid(metall::offset_ptr<const T>));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::void_pointer),
                    typeid(alloc_t::void_pointer));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::void_pointer),
                    typeid(metall::offset_ptr<void>));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::const_void_pointer),
                    typeid(alloc_t::const_void_pointer));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::const_void_pointer),
                    typeid(metall::offset_ptr<const void>));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::difference_type),
                    typeid(alloc_t::difference_type));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::size_type),
                    typeid(alloc_t::size_type));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<
                           alloc_t>::propagate_on_container_copy_assignment),
                    typeid(std::false_type));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<
                           alloc_t>::propagate_on_container_copy_assignment),
                    typeid(std::false_type));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<
                           alloc_t>::propagate_on_container_move_assignment),
                    typeid(std::false_type));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<
                           alloc_t>::propagate_on_container_move_assignment),
                    typeid(std::false_type));

    GTEST_ASSERT_EQ(
        typeid(std::allocator_traits<alloc_t>::propagate_on_container_swap),
        typeid(std::false_type));
    GTEST_ASSERT_EQ(
        typeid(std::allocator_traits<alloc_t>::propagate_on_container_swap),
        typeid(std::false_type));

    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::is_always_equal),
                    typeid(std::false_type));
    GTEST_ASSERT_EQ(typeid(std::allocator_traits<alloc_t>::is_always_equal),
                    typeid(std::false_type));

    using otherT = int;
    GTEST_ASSERT_EQ(
        typeid(std::allocator_traits<alloc_t>::rebind_alloc<otherT>),
        typeid(alloc_type<otherT>));
    GTEST_ASSERT_EQ(
        typeid(std::allocator_traits<alloc_t>::rebind_traits<otherT>),
        typeid(std::allocator_traits<alloc_type<otherT>>));
  }

  {
    struct T {
      T(int _a, double _b) : a(_a), b(_b) {}
      int a;
      double b;
    };
    using alloc_t = alloc_type<T>;

    metall::manager manager(metall::create_only, dir_path(), 1UL << 24UL);
    alloc_t alloc = manager.get_allocator<T>();

    {
      auto p = std::allocator_traits<alloc_t>::allocate(alloc, 1);
      GTEST_ASSERT_NE(p, nullptr);
      std::allocator_traits<alloc_t>::deallocate(alloc, p, 1);
    }

    {
      auto p = std::allocator_traits<alloc_t>::allocate(alloc, 1);
      std::allocator_traits<alloc_t>::construct(
          alloc, metall::to_raw_pointer(p), 10, 20.0);
      GTEST_ASSERT_EQ(p->a, 10);
      GTEST_ASSERT_EQ(p->b, 20.0);
      std::allocator_traits<alloc_t>::destroy(alloc, metall::to_raw_pointer(p));
    }

    GTEST_ASSERT_EQ(std::allocator_traits<alloc_t>::max_size(alloc),
                    alloc.max_size());

    auto a2 =
        std::allocator_traits<alloc_t>::select_on_container_copy_construction(
            alloc);
    GTEST_ASSERT_EQ(alloc, a2);
  }
}

TEST(StlAllocatorTest, Exception) {
  metall::manager manager(metall::create_only, dir_path(), 1UL << 24UL);

  alloc_type<int> allocator = manager.get_allocator<int>();

  // Turn off log temporary because the following exception test cases could
  // show error messages
  metall::logger::set_log_level(metall::logger::level_filter::critical);

  ASSERT_NO_THROW({ allocator.deallocate(allocator.allocate(1), 1); });

  ASSERT_THROW(
      { allocator.allocate(allocator.max_size() + 1); },
      std::bad_array_new_length);

  metall::logger::set_log_level(metall::logger::level_filter::error);
}

TEST(StlAllocatorTest, Container) {
  {
    metall::manager manager(metall::create_only, dir_path(), 1UL << 27UL);
    using element_type = std::pair<uint64_t, uint64_t>;

    boost::interprocess::vector<element_type, alloc_type<element_type>> vector(
        manager.get_allocator<>());
    for (uint64_t i = 0; i < 1024; ++i) {
      vector.emplace_back(element_type(i, i * 2));
    }
    for (uint64_t i = 0; i < 1024; ++i) {
      ASSERT_EQ(vector[i], element_type(i, i * 2));
    }
  }
}

TEST(StlAllocatorTest, NestedContainer) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, alloc_type<element_type>>;
  using map_type = boost::unordered_map<
      element_type,             // Key
      vector_type,              // Value
      std::hash<element_type>,  // Hash function
      std::equal_to<>,          // Equal function
      boost::container::scoped_allocator_adaptor<
          alloc_type<std::pair<const element_type, vector_type>>>>;

  {
    metall::manager manager(metall::create_only, dir_path(), 1UL << 27UL);

    map_type map(manager.get_allocator<>());
    for (uint64_t i = 0; i < 1024; ++i) {
      map[i % 8].push_back(i);
    }

    for (uint64_t i = 0; i < 1024; ++i) {
      ASSERT_EQ(map.at(i % 8)[i / 8], i);
    }
  }
}

TEST(StlAllocatorTest, PersistentConstructFind) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, alloc_type<element_type>>;

  {
    metall::manager manager(metall::create_only, dir_path(), 1UL << 27UL);

    int *a = manager.construct<int>("int")(10);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.construct<vector_type>("vector_type")(
        manager.get_allocator<vector_type>());
    vec->emplace_back(10);
    vec->emplace_back(20);
  }

  {
    metall::manager manager(metall::open_only, dir_path());

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
    metall::manager manager(metall::open_only, dir_path());
    ASSERT_TRUE(manager.destroy<int>("int"));
    ASSERT_FALSE(manager.destroy<int>("int"));

    ASSERT_TRUE(manager.destroy<vector_type>("vector_type"));
    ASSERT_FALSE(manager.destroy<vector_type>("vector_type"));
  }
}

TEST(StlAllocatorTest, PersistentConstructOrFind) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, alloc_type<element_type>>;

  {
    metall::manager manager(metall::create_only, dir_path(), 1UL << 27UL);
    int *a = manager.find_or_construct<int>("int")(10);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.find_or_construct<vector_type>("vector_type")(
        manager.get_allocator<vector_type>());
    vec->emplace_back(10);
    vec->emplace_back(20);
  }

  {
    metall::manager manager(metall::open_only, dir_path());

    int *a = manager.find_or_construct<int>("int")(20);
    ASSERT_EQ(*a, 10);

    vector_type *vec = manager.find_or_construct<vector_type>("vector_type")(
        manager.get_allocator<vector_type>());
    ASSERT_EQ(vec->at(0), 10);
    ASSERT_EQ(vec->at(1), 20);
  }

  {
    metall::manager manager(metall::open_only, dir_path());
    ASSERT_TRUE(manager.destroy<int>("int"));
    ASSERT_FALSE(manager.destroy<int>("int"));

    ASSERT_TRUE(manager.destroy<vector_type>("vector_type"));
    ASSERT_FALSE(manager.destroy<vector_type>("vector_type"));
  }
}

TEST(StlAllocatorTest, PersistentNestedContainer) {
  using element_type = uint64_t;
  using vector_type =
      boost::interprocess::vector<element_type, alloc_type<element_type>>;
  using map_type = boost::unordered_map<
      element_type,                 // Key
      vector_type,                  // Value
      std::hash<element_type>,      // Hash function
      std::equal_to<element_type>,  // Equal function
      boost::container::scoped_allocator_adaptor<
          alloc_type<std::pair<const element_type, vector_type>>>>;

  {
    metall::manager manager(metall::create_only, dir_path(), 1UL << 27UL);
    map_type *map =
        manager.construct<map_type>("map")(manager.get_allocator<>());
    (*map)[0].emplace_back(1);
    (*map)[0].emplace_back(2);
  }

  {
    metall::manager manager(metall::open_only, dir_path());
    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    ASSERT_EQ(map->at(0)[0], 1);
    ASSERT_EQ(map->at(0)[1], 2);
    (*map)[1].emplace_back(3);
  }

  {
    metall::manager manager(metall::open_read_only, dir_path());
    map_type *map;
    std::size_t n;
    std::tie(map, n) = manager.find<map_type>("map");

    ASSERT_EQ(map->at(0)[0], 1);
    ASSERT_EQ(map->at(0)[1], 2);
    ASSERT_EQ(map->at(1)[0], 3);
  }
}

// std::allocator_traits calls a.construct(p, args...) and a.destroy(p), with a
// raw pointer p, only if these calls are well-formed. Otherwise, it constructs
// with placement new and destroys with the destructor.
template <typename alloc_t, typename = void>
struct has_member_construct : std::false_type {};

template <typename alloc_t>
struct has_member_construct<
    alloc_t, std::void_t<decltype(std::declval<alloc_t &>().construct(
                 std::declval<typename alloc_t::value_type *>(),
                 std::declval<const typename alloc_t::value_type &>()))>>
    : std::true_type {};

template <typename alloc_t, typename = void>
struct has_member_destroy : std::false_type {};

template <typename alloc_t>
struct has_member_destroy<
    alloc_t, std::void_t<decltype(std::declval<alloc_t &>().destroy(
                 std::declval<typename alloc_t::value_type *>()))>>
    : std::true_type {};

struct trivially_copyable_type {
  std::uint64_t key;
  std::uint64_t value;
};

TEST(StlAllocatorTest, NoConstructDestroy) {
  // The allocator of a container of a trivially copyable type has no
  // construct() and destroy(), so std::allocator_traits constructs in place
  // and destroys with the destructor.
  using vector_type = metall::container::vector<trivially_copyable_type>;
  using alloc_t = vector_type::allocator_type;
  static_assert(std::is_trivially_copyable_v<trivially_copyable_type>);
  static_assert(std::is_same_v<alloc_t, alloc_type<trivially_copyable_type>>);
  static_assert(!has_member_construct<alloc_t>::value,
                "std::allocator_traits must not call construct() of "
                "metall::manager::allocator_type");
  static_assert(!has_member_destroy<alloc_t>::value,
                "std::allocator_traits must not call destroy() of "
                "metall::manager::allocator_type");
  static_assert(!has_member_construct<alloc_type<std::uint64_t>>::value);
  static_assert(!has_member_destroy<alloc_type<std::uint64_t>>::value);

  metall::manager manager(metall::create_only, dir_path(), 1UL << 24UL);
  vector_type vector(manager.get_allocator<>());
  for (std::uint64_t i = 0; i < 1024; ++i) {
    vector.push_back(trivially_copyable_type{i, i * 2});
  }
  vector.erase(vector.begin());
  ASSERT_EQ(vector.size(), 1023U);
  for (std::uint64_t i = 0; i < vector.size(); ++i) {
    ASSERT_EQ(vector[i].key, i + 1);
    ASSERT_EQ(vector[i].value, (i + 1) * 2);
  }
}

TEST(StlAllocatorTest, ConstructDestroyInPlace) {
  struct counted_type {
    explicit counted_type(int *count) : count(count) { ++*count; }
    ~counted_type() { --*count; }
    int *count;
  };
  using alloc_t = alloc_type<counted_type>;
  using traits = std::allocator_traits<alloc_t>;

  metall::manager manager(metall::create_only, dir_path(), 1UL << 24UL);
  alloc_t alloc = manager.get_allocator<counted_type>();

  int count = 0;
  auto p = traits::allocate(alloc, 1);
  traits::construct(alloc, metall::to_raw_pointer(p), &count);
  ASSERT_EQ(count, 1);
  ASSERT_EQ(p->count, &count);
  traits::destroy(alloc, metall::to_raw_pointer(p));
  ASSERT_EQ(count, 0);
  traits::deallocate(alloc, p, 1);
}
}  // namespace