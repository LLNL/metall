// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/kernel/named_object_directory.hpp>
#include "../test_utility.hpp"

namespace {

using directory_type = metall::kernel::named_object_directory<ssize_t, std::size_t, std::allocator<char>>;

TEST(NambedObjectDirectoryTest, Insert) {
  directory_type obj;

  ASSERT_TRUE(obj.insert("item1", 1, 1));
  ASSERT_TRUE(obj.insert("item2", 1, 1, "description2"));
}

// Should insert uniquely
TEST(NambedObjectDirectoryTest, UniqueInsert) {
  directory_type obj;

  obj.insert("item1", 1, 1);
  ASSERT_FALSE(obj.insert("item1", 1, 1));

  obj.insert("item2", 1, 1, "description2");
  ASSERT_FALSE(obj.insert("item2", 1, 1));
}

TEST(NambedObjectDirectoryTest, Count) {
  directory_type obj;

  ASSERT_EQ(obj.count("item1"), 0);
  obj.insert("item1", 1, 2);
  ASSERT_EQ(obj.count("item1"), 1);

  ASSERT_EQ(obj.count("item2"), 0);
  obj.insert("item2", 3, 4);
  ASSERT_EQ(obj.count("item1"), 1);
  ASSERT_EQ(obj.count("item2"), 1);
}

TEST(NambedObjectDirectoryTest, GetValue) {
  directory_type obj;

  typename directory_type::offset_type offset = 0;
  typename directory_type::length_type length = 0;

  // Fails before insertions
  ASSERT_FALSE(obj.get_offset("item1", &offset));
  ASSERT_FALSE(obj.get_length("item1", &length));
  obj.insert("item1", 1, 2);

  // Fails before insertions
  ASSERT_FALSE(obj.get_offset("item2", &offset));
  ASSERT_FALSE(obj.get_length("item2", &length));
  obj.insert("item2", 3, 4);

  // Get values correctly
  ASSERT_TRUE(obj.get_offset("item1", &offset));
  ASSERT_EQ(offset, 1);
  ASSERT_TRUE(obj.get_length("item1", &length));
  ASSERT_EQ(length, 2);

  // Get values correctly
  ASSERT_TRUE(obj.get_offset("item2", &offset));
  ASSERT_EQ(offset, 3);
  ASSERT_TRUE(obj.get_length("item2", &length));
  ASSERT_EQ(length, 4);
}

TEST(NambedObjectDirectoryTest, Description) {
  directory_type obj;

  directory_type::description_type buf;

  // Operations should fails for non-existing name
  ASSERT_FALSE(obj.get_description("item1", &buf));
  ASSERT_FALSE(obj.set_description("item1", buf));

  // Check empty description
  obj.insert("item1", 1, 1);
  ASSERT_TRUE(obj.get_description("item1", &buf));
  ASSERT_TRUE(buf.empty());

  // Set an description
  buf = "Description1";
  ASSERT_TRUE(obj.set_description("item1", buf));

  // Get an description
  directory_type::description_type buf2;
  ASSERT_TRUE(obj.get_description("item1", &buf2));
  ASSERT_EQ(buf, buf2);
}

TEST(NambedObjectDirectoryTest, Erase) {
  directory_type obj;

  ASSERT_EQ(obj.erase("item1"), 0);
  obj.insert("item1", 1, 2);

  ASSERT_EQ(obj.erase("item2"), 0);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(obj.erase("item1"), 1);
  ASSERT_EQ(obj.count("item1"), 0);
  ASSERT_EQ(obj.erase("item1"), 0);

  ASSERT_EQ(obj.erase("item2"), 1);
  ASSERT_EQ(obj.count("item2"), 0);
  ASSERT_EQ(obj.erase("item2"), 0);
}

TEST(NambedObjectDirectoryTest, KeyIterator) {
  directory_type obj;

  ASSERT_EQ(obj.keys_begin(), obj.keys_end());
  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  int count = 0;
  bool found1 = false;
  bool found2 = false;
  for (auto itr = obj.keys_begin(); itr != obj.keys_end(); ++itr) {
    ASSERT_TRUE(*itr == "item1" || *itr == "item2");
    found1 |= *itr == "item1";
    found2 |= *itr == "item2";
    ++count;
  }
  ASSERT_TRUE(found1);
  ASSERT_TRUE(found2);
  ASSERT_EQ(count, 2);

  obj.erase("item1");
  ASSERT_EQ(*(obj.keys_begin()), "item2");

  obj.erase("item2");
  ASSERT_EQ(obj.keys_begin(), obj.keys_end());
}

TEST(NambedObjectDirectoryTest, Serialize) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4, "description2");

  test_utility::create_test_dir();
  const auto file(test_utility::make_test_path());

  ASSERT_TRUE(obj.serialize(file.c_str()));
}

TEST(NambedObjectDirectoryTest, Deserialize) {
  test_utility::create_test_dir();
  const auto file(test_utility::make_test_path());

  {
    directory_type obj;
    obj.insert("item1", 1, 2);
    obj.insert("item2", 3, 4, "description2");
    obj.serialize(file.c_str());
  }

  {
    directory_type obj;
    ASSERT_TRUE(obj.deserialize(file.c_str()));

    typename directory_type::offset_type offset = 0;
    typename directory_type::length_type length = 0;
    directory_type::description_type description;

    // Get values correctly
    ASSERT_TRUE(obj.get_offset("item1", &offset));
    ASSERT_EQ(offset, 1);
    ASSERT_TRUE(obj.get_length("item1", &length));
    ASSERT_EQ(length, 2);
    ASSERT_TRUE(obj.get_description("item1", &description));
    ASSERT_TRUE(description.empty());


    // Get values correctly
    ASSERT_TRUE(obj.get_offset("item2", &offset));
    ASSERT_EQ(offset, 3);
    ASSERT_TRUE(obj.get_length("item2", &length));
    ASSERT_EQ(length, 4);
    ASSERT_TRUE(obj.get_description("item2", &description));
    ASSERT_EQ(description, "description2");

    // Key table is also restored
    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = obj.keys_begin(); itr != obj.keys_end(); ++itr) {
      ASSERT_TRUE(*itr == "item1" || *itr == "item2");
      found1 |= *itr == "item1";
      found2 |= *itr == "item2";
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);
  }
}

}