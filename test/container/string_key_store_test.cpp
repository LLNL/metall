// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <scoped_allocator>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/container/vector.hpp>
#include <metall/container/string_key_store.hpp>
#include "../test_utility.hpp"

#define METALL_CONTAINER_STRING_KEY_STORE_USE_SIMPLE_HASH

namespace {

namespace bip = boost::interprocess;

TEST (StringKeyStoreTest, DuplicateInsert) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);

  ASSERT_FALSE(store.unique());
  ASSERT_EQ(store.count("a"), 0);
  ASSERT_EQ(store.size(), 0);

  ASSERT_TRUE(store.insert("a"));
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.size(), 1);

  ASSERT_TRUE(store.insert("a"));
  ASSERT_EQ(store.count("a"), 2);
  ASSERT_EQ(store.size(), 2);

  ASSERT_TRUE(store.insert("b"));
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 3);

  std::string val("1");
  ASSERT_TRUE(store.insert("b", val));
  ASSERT_EQ(store.count("b"), 2);
  ASSERT_EQ(store.size(), 4);

  ASSERT_TRUE(store.insert("b", std::move(val)));
  ASSERT_EQ(store.count("b"), 3);
  ASSERT_EQ(store.size(), 5);
}

TEST (StringKeyStoreTest, UniqueInsert) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);

  ASSERT_TRUE(store.unique());
  ASSERT_EQ(store.count("a"), 0);
  ASSERT_EQ(store.size(), 0);

  ASSERT_TRUE(store.insert("a"));
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.size(), 1);

  ASSERT_FALSE(store.insert("a"));
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.size(), 1);

  ASSERT_TRUE(store.insert("b"));
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 2);

  std::string val("1");
  ASSERT_TRUE(store.insert("b", val));
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 2);

  ASSERT_TRUE(store.insert("b", std::move(val)));
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 2);
}

TEST (StringKeyStoreTest, CopyConstructorDuplicate) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  store.insert("a");
  store.insert("b");
  store.insert("b");

  auto store_copy(store);
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.count("b"), 2);
  ASSERT_EQ(store.size(), 3);

  ASSERT_EQ(store.get_allocator(), store_copy.get_allocator());
  ASSERT_EQ(store.unique(), store_copy.unique());
  ASSERT_EQ(store.hash_seed(), store_copy.hash_seed());

  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 2);
  ASSERT_EQ(store_copy.size(), 3);

  ASSERT_TRUE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 2);
  ASSERT_EQ(store_copy.size(), 4);
}

TEST (StringKeyStoreTest, CopyConstructorUnique) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  store.insert("a");
  store.insert("b");

  auto store_copy(store);
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 2);

  ASSERT_EQ(store.get_allocator(), store_copy.get_allocator());
  ASSERT_EQ(store.unique(), store_copy.unique());
  ASSERT_EQ(store.hash_seed(), store_copy.hash_seed());

  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 1);
  ASSERT_EQ(store_copy.size(), 2);

  ASSERT_FALSE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.size(), 2);
}

TEST (StringKeyStoreTest, CopyAsignmentDuplicate) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  store.insert("a");
  store.insert("b");
  store.insert("b");

  auto store_copy = store;
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.count("b"), 2);
  ASSERT_EQ(store.size(), 3);

  ASSERT_EQ(store.get_allocator(), store_copy.get_allocator());
  ASSERT_EQ(store.unique(), store_copy.unique());
  ASSERT_EQ(store.hash_seed(), store_copy.hash_seed());

  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 2);
  ASSERT_EQ(store_copy.size(), 3);

  ASSERT_TRUE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 2);
  ASSERT_EQ(store_copy.size(), 4);
}

TEST (StringKeyStoreTest, CopyAsignmentUnique) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  store.insert("a");
  store.insert("b");

  auto store_copy = store;
  ASSERT_EQ(store.count("a"), 1);
  ASSERT_EQ(store.count("b"), 1);
  ASSERT_EQ(store.size(), 2);

  ASSERT_EQ(store.get_allocator(), store_copy.get_allocator());
  ASSERT_EQ(store.unique(), store_copy.unique());
  ASSERT_EQ(store.hash_seed(), store_copy.hash_seed());

  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 1);
  ASSERT_EQ(store_copy.size(), 2);

  ASSERT_FALSE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.size(), 2);
}

TEST (StringKeyStoreTest, MoveConstructorDuplicate) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  store.insert("a");
  store.insert("b");
  store.insert("b");

  auto store_copy(std::move(store));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 2);
  ASSERT_EQ(store_copy.size(), 3);

  ASSERT_TRUE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 2);
  ASSERT_EQ(store_copy.size(), 4);
}

TEST (StringKeyStoreTest, MoveConstructorUnique) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  store.insert("a");
  store.insert("b");

  auto store_copy(std::move(store));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 1);
  ASSERT_EQ(store_copy.size(), 2);

  ASSERT_FALSE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.size(), 2);
}

TEST (StringKeyStoreTest, MoveAsignmentDuplicate) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  store.insert("a");
  store.insert("b");
  store.insert("b");

  auto store_copy = std::move(store);
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 2);
  ASSERT_EQ(store_copy.size(), 3);

  ASSERT_TRUE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 2);
  ASSERT_EQ(store_copy.size(), 4);
}

TEST (StringKeyStoreTest, MoveAsignmentUnique) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  store.insert("a");
  store.insert("b");

  auto store_copy = std::move(store);
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.count("b"), 1);
  ASSERT_EQ(store_copy.size(), 2);

  ASSERT_FALSE(store_copy.insert("a"));
  ASSERT_EQ(store_copy.count("a"), 1);
  ASSERT_EQ(store_copy.size(), 2);
}

TEST (StringKeyStoreTest, Clear) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  store.insert("a");
  store.insert("b", "0");
  store.clear();
  ASSERT_EQ(store.size(), 0);
}

TEST (StringKeyStoreTest, EraseMultipleWithKey) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  ASSERT_EQ(store.erase("a"), 0);
  store.insert("a");
  store.insert("b");
  store.insert("b");
  ASSERT_EQ(store.erase("c"), 0);
  ASSERT_EQ(store.erase("a"), 1);
  ASSERT_EQ(store.erase("a"), 0);
  ASSERT_EQ(store.erase("b"), 2);
  ASSERT_EQ(store.erase("b"), 0);
}

TEST (StringKeyStoreTest, EraseSingleWithKey) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  ASSERT_EQ(store.erase("a"), 0);
  store.insert("a");
  store.insert("b");
  store.insert("b");
  ASSERT_EQ(store.erase("c"), 0);
  ASSERT_EQ(store.erase("a"), 1);
  ASSERT_EQ(store.erase("a"), 0);
  ASSERT_EQ(store.erase("b"), 1);
  ASSERT_EQ(store.erase("b"), 0);
}

TEST (StringKeyStoreTest, EraseMultipleWithLocator) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  ASSERT_EQ(store.erase(store.find("a")), store.end());
  store.insert("a");
  store.insert("b");
  store.insert("b");
  ASSERT_EQ(store.erase(store.find("c")), store.end());

  auto itr = store.begin();
  ASSERT_NE(itr = store.erase(itr), store.end());
  ASSERT_NE(itr = store.erase(itr), store.end());
  ASSERT_EQ(itr = store.erase(itr), store.end());
}

TEST (StringKeyStoreTest, EraseSingleWithLocator) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  ASSERT_EQ(store.erase(store.find("a")), store.end());
  store.insert("a");
  store.insert("b");
  store.insert("b");
  ASSERT_EQ(store.erase(store.find("c")), store.end());

  auto itr = store.begin();
  ASSERT_NE(itr = store.erase(itr), store.end());
  ASSERT_EQ(itr = store.erase(itr), store.end());
}

TEST (StringKeyStoreTest, LocatorDuplicate) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(false, 111);
  ASSERT_EQ(store.begin(), store.end());
  ASSERT_EQ(store.find("a"), store.end());
  ASSERT_EQ(store.equal_range("a").first, store.end());
  ASSERT_EQ(store.equal_range("a").second, store.end());
  store.insert("a");
  store.insert("b");
  store.insert("b", "0");
  ASSERT_NE(store.begin(), store.end());

  ASSERT_EQ(store.key(store.find("a")), "a");
  ASSERT_EQ(store.value(store.find("a")), std::string());

  {
    auto a_range = store.equal_range("a");
    std::size_t a_count = 0;
    for (auto locator = a_range.first; locator != a_range.second; ++locator) {
      ASSERT_EQ(store.key(locator), "a");
      ASSERT_EQ(store.value(locator), std::string());
      ++a_count;
    }
    ASSERT_EQ(a_count, 1);
  }

  {
    auto b_range = store.equal_range("b");
    std::size_t b_count = 0;
    std::size_t b_default_value_count = 0;
    std::size_t b_with_value_count = 0;
    for (auto locator = b_range.first; locator != b_range.second; ++locator) {
      ASSERT_EQ(store.key(locator), "b");
      b_default_value_count += store.value(locator).empty();
      b_with_value_count += store.value(locator) == "0";
      ++b_count;
    }
    ASSERT_EQ(b_count, 2);
    ASSERT_EQ(b_default_value_count, 1);
    ASSERT_EQ(b_with_value_count, 1);
  }

  {
    std::size_t count = 0;
    std::size_t a_count = 0;
    std::size_t b_default_value_count = 0;
    std::size_t b_with_value_count = 0;
    for (auto locator = store.begin(); locator != store.end(); ++locator) {
      a_count += store.key(locator) == "a" && store.value(locator).empty();
      b_default_value_count += store.key(locator) == "b" && store.value(locator).empty();
      b_with_value_count += store.key(locator) == "b" && store.value(locator) == "0";
      ++count;
    }
    ASSERT_EQ(count, 3);
    ASSERT_EQ(a_count, 1);
    ASSERT_EQ(b_default_value_count, 1);
    ASSERT_EQ(b_with_value_count, 1);
  }
}

TEST (StringKeyStoreTest, LocatorUnique) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store(true, 111);
  ASSERT_EQ(store.begin(), store.end());
  ASSERT_EQ(store.find("a"), store.end());
  ASSERT_EQ(store.equal_range("a").first, store.end());
  ASSERT_EQ(store.equal_range("a").second, store.end());
  store.insert("a");
  store.insert("b");
  store.insert("b", "0");
  ASSERT_NE(store.begin(), store.end());

  ASSERT_EQ(store.key(store.find("a")), "a");
  ASSERT_EQ(store.value(store.find("a")), std::string());

  {
    auto a_range = store.equal_range("a");
    std::size_t a_count = 0;
    for (auto locator = a_range.first; locator != a_range.second; ++locator) {
      ASSERT_EQ(store.key(locator), "a");
      ASSERT_EQ(store.value(locator), std::string());
      ++a_count;
    }
    ASSERT_EQ(a_count, 1);
  }

  {
    auto b_range = store.equal_range("b");
    std::size_t b_count = 0;
    std::size_t b_default_value_count = 0;
    std::size_t b_with_value_count = 0;
    for (auto locator = b_range.first; locator != b_range.second; ++locator) {
      ASSERT_EQ(store.key(locator), "b");
      b_default_value_count += store.value(locator).empty();
      b_with_value_count += store.value(locator) == "0";
      ++b_count;
    }
    ASSERT_EQ(b_count, 1);
    ASSERT_EQ(b_default_value_count, 0);
    ASSERT_EQ(b_with_value_count, 1);
  }

  {
    std::size_t count = 0;
    std::size_t a_count = 0;
    std::size_t b_default_value_count = 0;
    std::size_t b_with_value_count = 0;
    for (auto locator = store.begin(); locator != store.end(); ++locator) {
      a_count += store.key(locator) == "a" && store.value(locator).empty();
      b_default_value_count += store.key(locator) == "b" && store.value(locator).empty();
      b_with_value_count += store.key(locator) == "b" && store.value(locator) == "0";
      ++count;
    }
    ASSERT_EQ(count, 2);
    ASSERT_EQ(a_count, 1);
    ASSERT_EQ(b_default_value_count, 0);
    ASSERT_EQ(b_with_value_count, 1);
  }
}

TEST (StringKeyStoreTest, MaxProbeDistance) {
  metall::container::string_key_store<int, std::allocator<std::byte>> store;
  ASSERT_EQ(store.max_id_probe_distance(), 0);
  store.insert("a");
  ASSERT_EQ(store.max_id_probe_distance(), 0);
  store.insert("b");
  ASSERT_LE(store.max_id_probe_distance(), 1);
}

TEST (StringKeyStoreTest, Rehash) {
  metall::container::string_key_store<std::string, std::allocator<std::byte>> store;
  store.insert("a", "0");
  store.insert("b", "1");
  store.insert("c", "2");
  store.rehash();
  ASSERT_EQ(store.value(store.find("a")), "0");
  ASSERT_EQ(store.value(store.find("b")), "1");
  ASSERT_EQ(store.value(store.find("c")), "2");
  store.erase("b");
  store.rehash();
  ASSERT_EQ(store.value(store.find("a")), "0");
  ASSERT_EQ(store.value(store.find("c")), "2");
}

TEST (StringKeyStoreTest, Persistence) {
  using value_type = boost::container::vector<int, bip::allocator<int, bip::managed_mapped_file::segment_manager>>;
  using store_type = metall::container::string_key_store<value_type,
                                                         bip::allocator<std::byte,
                                                                        bip::managed_mapped_file::segment_manager>>;
  const std::string file_path(test_utility::make_test_path());
  test_utility::create_test_dir();
  metall::mtlldetail::remove_file(file_path);

  // Create
  {
    bip::managed_mapped_file mfile(bip::create_only, file_path.c_str(), 1 << 20);
    auto *store = mfile.construct<store_type>(bip::unique_instance)(true, 111, mfile.get_allocator<std::byte>());
    store->insert("a");
    store->value(store->find("a")).push_back(10);
  }

  // Append
  {
    bip::managed_mapped_file mfile(bip::open_only, file_path.c_str());
    auto *store = mfile.find<store_type>(bip::unique_instance).first;

    ASSERT_EQ(store->size(), 1);
    ASSERT_EQ(store->value(store->find("a"))[0], 10);

    value_type vec(mfile.get_allocator<int>());
    vec.push_back(20);
    vec.push_back(30);
    store->insert("b", std::move(vec));
  }

  // Read only
  {
    bip::managed_mapped_file mfile(bip::open_read_only, file_path.c_str());
    auto *store = mfile.find<store_type>(bip::unique_instance).first;
    ASSERT_EQ(store->size(), 2);
    ASSERT_EQ(store->value(store->find("a"))[0], 10);
    ASSERT_EQ(store->value(store->find("b"))[0], 20);
    ASSERT_EQ(store->value(store->find("b"))[1], 30);
  }
}

}