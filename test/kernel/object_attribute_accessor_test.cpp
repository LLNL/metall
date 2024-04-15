// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"

#include <unordered_set>

#include <metall/metall.hpp>
#include "../test_utility.hpp"

namespace {
using namespace metall;

auto attr_accessor_named() {
  return manager::access_named_object_attribute(
      test_utility::make_test_path());
}

auto attr_accessor_unique() {
  return manager::access_unique_object_attribute(
      test_utility::make_test_path());
}

auto attr_accessor_anonymous() {
  return manager::access_anonymous_object_attribute(
      test_utility::make_test_path());
}

TEST(ObjectAttributeAccessorTest, Constructor) {
  manager::remove(test_utility::make_test_path());

  {
    manager mngr(create_only, test_utility::make_test_path(),
                 1ULL << 30ULL);
  }

  ASSERT_TRUE(attr_accessor_named().good());
  ASSERT_TRUE(attr_accessor_unique().good());
  ASSERT_TRUE(attr_accessor_anonymous().good());
}

TEST(ObjectAttributeAccessorTest, NumObjects) {
  manager::remove(test_utility::make_test_path());

  {
    manager mngr(create_only, test_utility::make_test_path(),
                 1ULL << 30ULL);
  }

  {
    GTEST_ASSERT_EQ(attr_accessor_named().num_objects(), 0);
    GTEST_ASSERT_EQ(attr_accessor_unique().num_objects(), 0);
    GTEST_ASSERT_EQ(attr_accessor_anonymous().num_objects(), 0);
  }

  {
    manager mngr(open_only, test_utility::make_test_path());
    mngr.construct<int>("int1")();
    mngr.construct<int>("int2")();
    mngr.construct<float>(unique_instance)();
    mngr.construct<float>(anonymous_instance)();
  }

  {
    GTEST_ASSERT_EQ(attr_accessor_named().num_objects(), 2);
    GTEST_ASSERT_EQ(attr_accessor_unique().num_objects(), 1);
    GTEST_ASSERT_EQ(attr_accessor_anonymous().num_objects(), 1);
  }
}

TEST(ObjectAttributeAccessorTest, Count) {
  manager::remove(test_utility::make_test_path());

  {
    manager mngr(create_only, test_utility::make_test_path(),
                 1ULL << 30ULL);
  }

  {
    GTEST_ASSERT_EQ(attr_accessor_named().count("int1"), 0);
    GTEST_ASSERT_EQ(attr_accessor_unique().count<float>(unique_instance), 0);
  }

  {
    manager mngr(open_only, test_utility::make_test_path().c_str());
    mngr.construct<int>("int1")();
    mngr.construct<float>(unique_instance)();
  }

  {
    GTEST_ASSERT_EQ(attr_accessor_named().count("int1"), 1);
    GTEST_ASSERT_EQ(
        attr_accessor_unique().template count<float>(unique_instance), 1);
  }
}

TEST(ObjectAttributeAccessorTest, Find) {
  manager::remove(test_utility::make_test_path().c_str());

  {
    manager mngr(create_only, test_utility::make_test_path().c_str(),
                 1ULL << 30ULL);
  }

  {
    auto aan = attr_accessor_named();
    GTEST_ASSERT_EQ(aan.find("int1"), aan.end());

    auto aau = attr_accessor_unique();
    GTEST_ASSERT_EQ(aau.template find<float>(unique_instance), aau.end());
  }

  {
    manager mngr(open_only, test_utility::make_test_path().c_str());
    mngr.construct<int>("int1")();
    mngr.construct<float>(unique_instance)();
  }

  {
    GTEST_ASSERT_EQ(attr_accessor_named().find("int1")->name(), "int1");
    GTEST_ASSERT_EQ(
        attr_accessor_unique().template find<float>(unique_instance)->name(),
        typeid(float).name());
  }
}

TEST(ObjectAttributeAccessorTest, Iterator) {
  manager::remove(test_utility::make_test_path().c_str());

  {
    manager mngr(create_only, test_utility::make_test_path().c_str(),
                 1ULL << 30ULL);
  }

  {
    auto aan = attr_accessor_named();
    GTEST_ASSERT_EQ(aan.begin(), aan.end());

    auto aau = attr_accessor_unique();
    GTEST_ASSERT_EQ(aau.begin(), aau.end());

    auto aaa = attr_accessor_unique();
    GTEST_ASSERT_EQ(aaa.begin(), aaa.end());
  }

  ptrdiff_t anony_off_obj1 = 0;
  ptrdiff_t anony_off_obj2 = 0;
  {
    manager mngr(open_only, test_utility::make_test_path().c_str());
    mngr.construct<int>("int1")();
    mngr.construct<float>("float1")();

    mngr.construct<int>(unique_instance)();
    mngr.construct<float>(unique_instance)();

    anony_off_obj1 =
        reinterpret_cast<char *>(mngr.construct<int>(anonymous_instance)()) -
        reinterpret_cast<const char *>(mngr.get_address());
    anony_off_obj2 =
        reinterpret_cast<char *>(mngr.construct<float>(anonymous_instance)()) -
        reinterpret_cast<const char *>(mngr.get_address());
  }

  {
    auto aan = attr_accessor_named();
    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = aan.begin(); itr != aan.end(); ++itr) {
      found1 |= (itr->name() == "int1" && itr->is_type<int>());
      found2 |= (itr->name() == "float1" && itr->is_type<float>());
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);
  }

  {
    auto aau = attr_accessor_unique();
    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = aau.begin(); itr != aau.end(); ++itr) {
      found1 |= (itr->name() == typeid(int).name() && itr->is_type<int>());
      found2 |= (itr->name() == typeid(float).name() && itr->is_type<float>());
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);
  }

  {
    auto aaa = attr_accessor_anonymous();
    int count = 0;
    bool found1 = false;
    bool found2 = false;
    for (auto itr = aaa.begin(); itr != aaa.end(); ++itr) {
      found1 |= (itr->offset() == anony_off_obj1 && itr->is_type<int>());
      found2 |= (itr->offset() == anony_off_obj2 && itr->is_type<float>());
      ++count;
    }
    ASSERT_TRUE(found1);
    ASSERT_TRUE(found2);
    ASSERT_EQ(count, 2);
  }
}

TEST(ObjectAttributeAccessorTest, Description) {
  manager::remove(test_utility::make_test_path().c_str());

  {
    manager mngr(create_only, test_utility::make_test_path().c_str(),
                 1ULL << 30ULL);
  }

  {
    ASSERT_FALSE(attr_accessor_named().set_description("int1", "desc1"));
    ASSERT_FALSE(attr_accessor_unique().set_description<float>(unique_instance,
                                                               "desc2"));
  }

  {
    manager mngr(open_only, test_utility::make_test_path().c_str());
    mngr.construct<int>("int1")();
    mngr.construct<float>(unique_instance)();
  }

  {
    ASSERT_TRUE(attr_accessor_named().set_description("int1", "desc1"));
    ASSERT_TRUE(attr_accessor_unique().set_description<float>(unique_instance,
                                                              "desc2"));
  }

  {
    manager mngr(open_only, test_utility::make_test_path().c_str());
    std::string buf1;
    ASSERT_TRUE(
        mngr.get_instance_description(mngr.find<int>("int1").first, &buf1));
    ASSERT_STREQ(buf1.c_str(), "desc1");

    std::string buf2;
    ASSERT_TRUE(mngr.get_instance_description(
        mngr.find<float>(unique_instance).first, &buf2));
    ASSERT_STREQ(buf2.c_str(), "desc2");
  }
}
}  // namespace