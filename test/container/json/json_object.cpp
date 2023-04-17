// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/json/json.hpp>

namespace mj = metall::json;

namespace {

using object_type = mj::object<std::allocator<std::byte>>;

TEST(JSONObjectTest, Constructor) {
  object_type obj;
  object_type obj_with_alloc(std::allocator<std::byte>{});
  object_type cp(obj);
  object_type mv(std::move(obj));
}

TEST(JSONObjectTest, Brackets) {
  object_type obj;

  obj["0"].emplace_bool() = true;
  obj["0123456789"].emplace_uint64() = 10;
  GTEST_ASSERT_TRUE(obj["0"].as_bool());
  GTEST_ASSERT_EQ(obj["0123456789"].as_uint64(), 10);

  // Override
  obj["0123456789"].emplace_double() = 20.5;
  GTEST_ASSERT_EQ(obj["0123456789"].as_double(), 20.5);

  const auto cnt_obj(obj);
  GTEST_ASSERT_TRUE(cnt_obj["0"].as_bool());
  GTEST_ASSERT_EQ(cnt_obj["0123456789"].as_double(), 20.5);
}

TEST(JSONObjectTest, ContainsAndCount) {
  object_type obj;

  GTEST_ASSERT_FALSE(obj.contains("0"));
  GTEST_ASSERT_EQ(obj.count("0"), 0);
  obj["0"].emplace_bool() = true;
  GTEST_ASSERT_TRUE(obj.contains("0"));
  GTEST_ASSERT_EQ(obj.count("0"), 1);

  GTEST_ASSERT_FALSE(obj.contains("0123456789"));
  GTEST_ASSERT_EQ(obj.count("0123456789"), 0);
  obj["0123456789"].emplace_uint64() = 10;
  GTEST_ASSERT_TRUE(obj.contains("0123456789"));
  GTEST_ASSERT_EQ(obj.count("0123456789"), 1);

  obj["0"].emplace_bool() = true;
  GTEST_ASSERT_TRUE(obj.contains("0"));
  GTEST_ASSERT_EQ(obj.count("0"), 1);
}

TEST(JSONObjectTest, At) {
  object_type obj;

  obj["0"].emplace_bool() = true;
  obj["0123456789"].emplace_uint64() = 10;
  GTEST_ASSERT_TRUE(obj.at("0").as_bool());
  GTEST_ASSERT_EQ(obj.at("0123456789").as_uint64(), 10);

  const auto cnt_obj(obj);
  GTEST_ASSERT_TRUE(cnt_obj.at("0").as_bool());
  GTEST_ASSERT_EQ(cnt_obj.at("0123456789").as_uint64(), 10);
}

TEST(JSONObjectTest, Find) {
  object_type obj;

  GTEST_ASSERT_EQ(obj.find("0"), obj.end());
  obj["0"].emplace_bool() = true;
  GTEST_ASSERT_NE(obj.find("0"), obj.end());
  GTEST_ASSERT_EQ(obj.find("0")->key(), "0");
  GTEST_ASSERT_TRUE(obj.find("0")->value().as_bool());

  GTEST_ASSERT_EQ(obj.find("0123456789"), obj.end());
  obj["0123456789"].emplace_uint64() = 10;
  GTEST_ASSERT_NE(obj.find("0123456789"), obj.end());
  GTEST_ASSERT_EQ(obj.find("0123456789")->key(), "0123456789");
  GTEST_ASSERT_EQ(obj.find("0123456789")->value().as_uint64(), 10);

  const auto cnt_obj(obj);
  GTEST_ASSERT_NE(cnt_obj.find("0"), cnt_obj.end());
  GTEST_ASSERT_EQ(cnt_obj.find("0")->key(), "0");
  GTEST_ASSERT_TRUE(cnt_obj.find("0")->value().as_bool());
  GTEST_ASSERT_NE(cnt_obj.find("0123456789"), cnt_obj.end());
  GTEST_ASSERT_EQ(cnt_obj.find("0123456789")->key(), "0123456789");
  GTEST_ASSERT_EQ(cnt_obj.find("0123456789")->value().as_uint64(), 10);
}

TEST(JSONObjectTest, BeginAndEnd) {
  object_type obj;

  GTEST_ASSERT_EQ(obj.begin(), obj.end());
  obj["0"].emplace_bool() = true;
  GTEST_ASSERT_NE(obj.begin(), obj.end());
  GTEST_ASSERT_EQ(obj.begin()->key(), "0");

  obj["0123456789"].emplace_uint64() = 10;

  std::size_t count = 0;
  for (auto &elem : obj) {
    GTEST_ASSERT_TRUE(elem.key() == "0" || elem.key() == "0123456789");
    if (elem.key() == "0123456789") elem.value().emplace_double() = 20.5;
    ++count;
  }
  GTEST_ASSERT_EQ(count, 2);
  GTEST_ASSERT_EQ(obj["0123456789"].as_double(), 20.5);

  count = 0;
  const auto cnt_obj(obj);
  for (const auto &elem : cnt_obj) {
    GTEST_ASSERT_TRUE(elem.key() == "0" || elem.key() == "0123456789");
    ++count;
  }
  GTEST_ASSERT_EQ(count, 2);
}

TEST(JSONObjectTest, Size) {
  object_type obj;

  GTEST_ASSERT_EQ(obj.size(), 0);

  obj["0"].emplace_bool() = true;
  GTEST_ASSERT_EQ(obj.size(), 1);

  obj["0123456789"].emplace_uint64() = 10;
  GTEST_ASSERT_EQ(obj.size(), 2);

  const auto cnt_obj(obj);
  GTEST_ASSERT_EQ(cnt_obj.size(), 2);
}

TEST(JSONObjectTest, Erase) {
  object_type obj;

  obj["0"].emplace_bool() = true;
  obj["0123456789"].emplace_uint64() = 10;
  obj["2"].emplace_double() = 20.5;

  obj.erase("0");
  GTEST_ASSERT_FALSE(obj.contains("0"));
  GTEST_ASSERT_EQ(obj.size(), 2);

  auto itr = obj.find("0123456789");
  GTEST_ASSERT_EQ(obj.erase(itr)->key(), "2");
  GTEST_ASSERT_FALSE(obj.contains("0123456789"));
  GTEST_ASSERT_EQ(obj.size(), 1);

  const auto &const_ref = obj;
  auto const_itr = const_ref.find("2");
  auto next_pos = obj.erase(const_itr);
  GTEST_ASSERT_EQ(next_pos, obj.end());
  GTEST_ASSERT_FALSE(obj.contains("2"));
  GTEST_ASSERT_EQ(obj.size(), 0);
}

TEST(JSONObjectTest, Equal) {
  object_type obj;
  obj["0"].emplace_bool() = true;
  obj["0123456789"].emplace_uint64() = 10;
  auto obj_cpy(obj);
  GTEST_ASSERT_TRUE(obj == obj_cpy);
  GTEST_ASSERT_FALSE(obj != obj_cpy);

  obj["0"].as_bool() = false;
  GTEST_ASSERT_FALSE(obj == obj_cpy);
  GTEST_ASSERT_TRUE(obj != obj_cpy);
}
}  // namespace