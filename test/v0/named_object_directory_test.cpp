// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/v0/kernel/named_object_directory.hpp>

namespace {

using directory_type = metall::v0::kernel::named_object_directory<ssize_t, std::size_t, std::allocator<char>>;
using key_type = std::string;

TEST(NambedObjectDirectoryTest, UniqueInsert) {
  std::allocator<char> allocator;
  directory_type obj(allocator);

  ASSERT_TRUE(obj.insert("item1", 1, 1));

  key_type name2{"item2"};
  ASSERT_TRUE(obj.insert(name2, 1, 1));

  key_type name3{"item3"};
  ASSERT_TRUE(obj.insert(std::move(name3), 1, 1));
}

TEST(NambedObjectDirectoryTest, DuplicateInsert) {
  std::allocator<char> allocator;
  directory_type obj(allocator);

  obj.insert("item1", 1, 1);
  ASSERT_FALSE(obj.insert("item1", 1, 1));

  key_type name2{"item2"};
  obj.insert(name2, 1, 1);
  ASSERT_FALSE(obj.insert(name2, 1, 1));

  key_type name3_1{"item3"};
  obj.insert(std::move(name3_1), 1, 1);
  key_type name3_2{"item3"};
  ASSERT_FALSE(obj.insert(std::move(name3_2), 1, 1));
}

TEST(NambedObjectDirectoryTest, Find) {
  std::allocator<char> allocator;
  directory_type obj(allocator);

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(std::get<1>(obj.find("item1")->second), 1);
  ASSERT_EQ(std::get<2>(obj.find("item1")->second), 2);
  ASSERT_EQ(std::get<1>(obj.find("item2")->second), 3);
  ASSERT_EQ(std::get<2>(obj.find("item2")->second), 4);
  ASSERT_EQ(obj.find("item3"), obj.end());
}

TEST(NambedObjectDirectoryTest, FindAndErase) {
  std::allocator<char> allocator;
  directory_type obj(allocator);

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(std::get<1>(obj.find("item1")->second), 1);
  ASSERT_EQ(std::get<2>(obj.find("item1")->second), 2);
  obj.erase(obj.find("item1"));
  ASSERT_EQ(obj.find("item1"), obj.end());

  ASSERT_EQ(std::get<1>(obj.find("item2")->second), 3);
  ASSERT_EQ(std::get<2>(obj.find("item2")->second), 4);
  obj.erase(obj.find("item2"));
  ASSERT_EQ(obj.find("item2"), obj.end());
}

TEST(NambedObjectDirectoryTest, Serialize) {
  std::allocator<char> allocator;
  directory_type obj(allocator);

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_TRUE(obj.serialize("./named_object_directory_test_file"));
}

TEST(NambedObjectDirectoryTest, Deserialize) {
  {
    std::allocator<char> allocator;
    directory_type obj(allocator);
    obj.insert("item1", 1, 2);
    obj.insert("item2", 3, 4);
    obj.serialize("./named_object_directory_test_file");
  }

  {
    std::allocator<char> allocator;
    directory_type obj(allocator);
    ASSERT_TRUE(obj.deserialize("./named_object_directory_test_file"));
    ASSERT_EQ(std::get<1>(obj.find("item1")->second), 1);
    ASSERT_EQ(std::get<2>(obj.find("item1")->second), 2);
    ASSERT_EQ(std::get<1>(obj.find("item2")->second), 3);
    ASSERT_EQ(std::get<2>(obj.find("item2")->second), 4);
  }
}

}