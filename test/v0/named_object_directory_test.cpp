// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <metall/v0/kernel/named_object_directory.hpp>

namespace {

using directory_type = metall::v0::kernel::named_object_directory<ssize_t, std::size_t>;
using key_type = typename directory_type::key_type;
using mapped_type = typename directory_type::mapped_type;

TEST(NambedObjectDirectoryTest, UniqueInsert) {
  directory_type obj;

  ASSERT_TRUE(obj.insert("item1", 1, 1));

  key_type name2{"item2"};
  ASSERT_TRUE(obj.insert(name2, 1, 1));

  key_type name3{"item3"};
  ASSERT_TRUE(obj.insert(std::move(name3), 1, 1));
}

TEST(NambedObjectDirectoryTest, DuplicateInsert) {
  directory_type obj;

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
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(obj.find("item1")->second, mapped_type(1, 2));
  ASSERT_EQ(obj.find("item2")->second, mapped_type(3, 4));
  ASSERT_EQ(obj.find("item3"), obj.end());
}

TEST(NambedObjectDirectoryTest, FindAndErase) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(obj.find("item1")->second, mapped_type(1, 2));
  obj.erase(obj.find("item1"));
  ASSERT_EQ(obj.find("item1"), obj.end());

  ASSERT_EQ(obj.find("item2")->second, mapped_type(3, 4));
  obj.erase(obj.find("item2"));
  ASSERT_EQ(obj.find("item2"), obj.end());
}

TEST(NambedObjectDirectoryTest, Serialize) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_TRUE(obj.serialize("./named_object_directory_test_file"));
}

TEST(NambedObjectDirectoryTest, Deserialize) {
  {
    directory_type obj;
    obj.insert("item1", 1, 2);
    obj.insert("item2", 3, 4);
    obj.serialize("./named_object_directory_test_file");
  }

  {
    directory_type obj;
    ASSERT_TRUE(obj.deserialize("./named_object_directory_test_file"));
    ASSERT_EQ(obj.find("item1")->second, mapped_type(1, 2));
    ASSERT_EQ(obj.find("item2")->second, mapped_type(3, 4));
  }
}

}