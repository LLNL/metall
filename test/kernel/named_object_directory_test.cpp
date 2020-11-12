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
  ASSERT_TRUE(obj.insert("item2", 1, 1, "attribute2"));
}

// Should insert uniquely
TEST(NambedObjectDirectoryTest, UniqueInsert) {
  directory_type obj;

  obj.insert("item1", 1, 1);
  ASSERT_FALSE(obj.insert("item1", 1, 1));

  obj.insert("item2", 1, 1, "attribute2");
  ASSERT_FALSE(obj.insert("item2", 1, 1));
}

TEST(NambedObjectDirectoryTest, Find) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(std::get<1>(obj.find("item1")->second), 1);
  ASSERT_EQ(std::get<2>(obj.find("item1")->second), 2);

  ASSERT_EQ(std::get<1>(obj.find("item2")->second), 3);
  ASSERT_EQ(std::get<2>(obj.find("item2")->second), 4);

  ASSERT_EQ(obj.find("item3"), obj.end());
}

TEST(NambedObjectDirectoryTest, Count) {
  directory_type obj;

  ASSERT_EQ(obj.count("item1"), 0);
  obj.insert("item1", 1, 2);
  ASSERT_EQ(obj.count("item1"), 1);

  ASSERT_EQ(obj.count("item2"), 0);
  obj.insert("item2", 3, 4);
  ASSERT_EQ(obj.count("item2"), 1);
}

TEST(NambedObjectDirectoryTest, Attribute) {
  directory_type obj;

  directory_type::attribute_type buf;

  // Operations should fails for non-existing name
  ASSERT_FALSE(obj.get_attribute("item1", &buf));
  ASSERT_FALSE(obj.set_attribute("item1", buf));

  // Check empty attribute
  obj.insert("item1", 1, 1);
  ASSERT_TRUE(obj.get_attribute("item1", &buf));
  ASSERT_TRUE(buf.empty());

  // Set an attribute
  buf = "Attribute1";
  ASSERT_TRUE(obj.set_attribute("item1", buf));

  // Get an attribute
  directory_type::attribute_type buf2;
  ASSERT_TRUE(obj.get_attribute("item1", &buf2));
  ASSERT_EQ(buf, buf2);
}

TEST(NambedObjectDirectoryTest, Erase) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4);

  ASSERT_EQ(obj.erase(obj.find("item1")), 1);
  ASSERT_EQ(obj.find("item1"), obj.end());
  ASSERT_EQ(obj.count("item1"), 0);
  ASSERT_NE(obj.find("item2"), obj.end());
  ASSERT_EQ(obj.count("item2"), 1);
  ASSERT_EQ(obj.erase(obj.find("item1")), 0);

  ASSERT_EQ(obj.erase(obj.find("item2")), 1);
  ASSERT_EQ(obj.find("item1"), obj.end());
  ASSERT_EQ(obj.count("item1"), 0);
  ASSERT_EQ(obj.find("item2"), obj.end());
  ASSERT_EQ(obj.count("item2"), 0);
  ASSERT_EQ(obj.erase(obj.find("item2")), 0);
}

TEST(NambedObjectDirectoryTest, Serialize) {
  directory_type obj;

  obj.insert("item1", 1, 2);
  obj.insert("item2", 3, 4, "attribute2");

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
    obj.insert("item2", 3, 4, "attribute2");
    obj.serialize(file.c_str());
  }

  {
      directory_type obj;
    ASSERT_TRUE(obj.deserialize(file.c_str()));

    ASSERT_EQ(std::get<1>(obj.find("item1")->second), 1);
    ASSERT_EQ(std::get<2>(obj.find("item1")->second), 2);

    ASSERT_EQ(std::get<1>(obj.find("item2")->second), 3);
    ASSERT_EQ(std::get<2>(obj.find("item2")->second), 4);
    ASSERT_EQ(std::get<3>(obj.find("item2")->second), "attribute2");
  }
}

}