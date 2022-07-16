// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include "gtest/gtest.h"
#include <memory>
#include <metall/container/experimental/json/json.hpp>

using namespace metall::container::experimental;

namespace {

std::string json_string = R"(
      {
        "pi": 3.141,
        "happy": true,
        "name": "Alice",
        "nothing": null,
        "answer": {
          "everything": 42
        },
        "list": [1, 0, 2],
        "object": {
          "currency": "USD",
          "value": 42.99
        }
      }
    )";

TEST (JSONValueTest, Constructor) {
  json::value val;
  json::value val_with_alloc(std::allocator<std::byte>{});
  json::value cp(val);
  json::value mv(std::move(val));
}

TEST (JSONValueTest, Assign) {
  json::value val;
  {
    val = nullptr;
    GTEST_ASSERT_TRUE(val.is_null());
  }

  {
    val = true;
    GTEST_ASSERT_TRUE(val.is_bool());
    GTEST_ASSERT_TRUE(val.as_bool());
  }

  {
    val = (signed char)-1;
    GTEST_ASSERT_TRUE(val.is_int64());
    GTEST_ASSERT_EQ(val.as_int64(), (signed char)-1);
  }

  {
    val = (int)-3;
    GTEST_ASSERT_TRUE(val.is_int64());
    GTEST_ASSERT_EQ(val.as_int64(), (int)-3);
  }

  {
    val = (long)-4;
    GTEST_ASSERT_TRUE(val.is_int64());
    GTEST_ASSERT_EQ(val.as_int64(), (long)-4);
  }

  {
    val = (long long)-5;
    GTEST_ASSERT_TRUE(val.is_int64());
    GTEST_ASSERT_EQ(val.as_int64(), (long long)-5);
  }

  {
    val = (unsigned char)1;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), (unsigned char)1);
  }

  {
    val = (unsigned short)2;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), (unsigned short)2);
  }

  {
    val = (unsigned int)3;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), (unsigned int)3);
  }

  {
    val = (unsigned long)4;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), (unsigned long)4);
  }

  {
    val = (unsigned long long)5;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), (unsigned long)5);
  }

  {
    val = (double)1.5;
    GTEST_ASSERT_TRUE(val.is_double());
    GTEST_ASSERT_EQ(val.as_double(), 1.5);
  }

  {
    std::string_view s = "string1";
    val = s;
    GTEST_ASSERT_TRUE(val.is_string());
    GTEST_ASSERT_EQ(val.as_string(), "string1");
  }

  {
    val = "string2";
    GTEST_ASSERT_TRUE(val.is_string());
    GTEST_ASSERT_EQ(val.as_string(), "string2");
  }

  {
    json::value<>::string_type s("string3");
    val = s;
    GTEST_ASSERT_TRUE(val.is_string());
    GTEST_ASSERT_EQ(val.as_string(), "string3");
  }

  {
    json::array ar;
    ar.resize(2);
    ar[0] = 1;
    ar[1] = 2;

    val = ar;
    GTEST_ASSERT_TRUE(val.is_array());
    GTEST_ASSERT_EQ(val.as_array()[0], 1);
    GTEST_ASSERT_EQ(val.as_array()[1], 2);
  }

  {
    json::array ar;
    ar.resize(2);
    ar[0] = 3;
    ar[1] = 4;

    val = std::move(ar);
    GTEST_ASSERT_TRUE(val.is_array());
    GTEST_ASSERT_EQ(val.as_array()[0], 3);
    GTEST_ASSERT_EQ(val.as_array()[1], 4);
  }

  {
    json::value<>::object_type oj;
    oj["val"] = 1.5;

    val = oj;
    GTEST_ASSERT_TRUE(val.is_object());
    GTEST_ASSERT_TRUE(val.as_object()["val"].is_double());
    GTEST_ASSERT_EQ(val.as_object()["val"].as_double(), 1.5);
  }

  {
    json::value<>::object_type oj;
    oj["val"] = 2.5;

    val = std::move(oj);
    GTEST_ASSERT_TRUE(val.is_object());
    GTEST_ASSERT_TRUE(val.as_object()["val"].is_double());
    GTEST_ASSERT_EQ(val.as_object()["val"].as_double(), 2.5);
  }
}

TEST (JSONValueTest, Emplace) {
  json::value val;
  {
    val.emplace_null();
    GTEST_ASSERT_TRUE(val.is_null());
  }

  {
    val.emplace_bool() = true;
    GTEST_ASSERT_TRUE(val.is_bool());
    GTEST_ASSERT_TRUE(val.as_bool());
  }

  {
    val.emplace_int64() = -1;
    GTEST_ASSERT_TRUE(val.is_int64());
    GTEST_ASSERT_EQ(val.as_int64(), (int64_t)-1);
  }

  {
    val.emplace_uint64() = 2;
    GTEST_ASSERT_TRUE(val.is_uint64());
    GTEST_ASSERT_EQ(val.as_uint64(), 2);
  }

  {
    val.emplace_double() = -1.5;
    GTEST_ASSERT_TRUE(val.is_double());
    GTEST_ASSERT_EQ(val.as_double(), -1.5);
  }

  {
    json::value<>::string_type s("string3");
    val.emplace_string() = s;
    GTEST_ASSERT_TRUE(val.is_string());
    GTEST_ASSERT_EQ(val.as_string(), "string3");
  }

  {
    json::array ar;
    ar.resize(2);
    ar[0] = 1;
    ar[1] = 2;

    val.emplace_array() = ar;
    GTEST_ASSERT_TRUE(val.is_array());
    GTEST_ASSERT_EQ(val.as_array()[0], 1);
    GTEST_ASSERT_EQ(val.as_array()[1], 2);
  }

  {
    json::array ar;
    ar.resize(2);
    ar[0] = 3;
    ar[1] = 4;

    val.emplace_array() = std::move(ar);
    GTEST_ASSERT_TRUE(val.is_array());
    GTEST_ASSERT_EQ(val.as_array()[0], 3);
    GTEST_ASSERT_EQ(val.as_array()[1], 4);
  }

  {
    json::value<>::object_type oj;
    oj["val"] = 1.5;

    val.emplace_object() = oj;
    GTEST_ASSERT_TRUE(val.is_object());
    GTEST_ASSERT_TRUE(val.as_object()["val"].is_double());
    GTEST_ASSERT_EQ(val.as_object()["val"].as_double(), 1.5);
  }

  {
    json::value<>::object_type oj;
    oj["val"] = 2.5;

    val.emplace_object() = std::move(oj);
    GTEST_ASSERT_TRUE(val.is_object());
    GTEST_ASSERT_TRUE(val.as_object()["val"].is_double());
    GTEST_ASSERT_EQ(val.as_object()["val"].as_double(), 2.5);
  }
}

TEST (JSONValueTest, Parse) {
  auto jv = json::parse(json_string);
  GTEST_ASSERT_EQ(jv.as_object()["pi"].as_double(), 3.141);
  GTEST_ASSERT_TRUE(jv.as_object()["happy"].as_bool());
  GTEST_ASSERT_EQ(jv.as_object()["name"].as_string(), "Alice");
  GTEST_ASSERT_TRUE(jv.as_object()["nothing"].is_null());
  GTEST_ASSERT_EQ(jv.as_object()["answer"].as_object()["everything"], 42);
  GTEST_ASSERT_EQ(jv.as_object()["list"].as_array()[0], 1);
  GTEST_ASSERT_EQ(jv.as_object()["list"].as_array()[1], 0);
  GTEST_ASSERT_EQ(jv.as_object()["list"].as_array()[2], 2);
  GTEST_ASSERT_EQ(jv.as_object()["object"].as_object()["currency"].as_string(), "USD");
  GTEST_ASSERT_EQ(jv.as_object()["object"].as_object()["value"].as_double(), 42.99);
}

TEST (JSONValueTest, Equal) {
  auto jv1 = json::parse(json_string);
  auto jv2 = json::parse(json_string);
  GTEST_ASSERT_EQ(jv1, jv2);

  jv1.as_object()["object"].as_object()["currency"].as_string() = "JPY";
  GTEST_ASSERT_NE(jv1, jv2);
}

TEST (JSONValueTest, EqualBool) {
  auto jv = json::value();
  jv.emplace_bool() = true;
  GTEST_ASSERT_EQ(jv, true);
  GTEST_ASSERT_NE(jv, -10);
  GTEST_ASSERT_NE(jv, 10);
  GTEST_ASSERT_NE(jv, 10.0);
}

TEST (JSONValueTest, EqualInt64) {
  auto jv = json::value();
  jv.emplace_int64() = -10;
  GTEST_ASSERT_NE(jv, true);
  GTEST_ASSERT_EQ(jv, -10);
  GTEST_ASSERT_NE(jv, 10);
  GTEST_ASSERT_NE(jv, 10.0);
}

TEST (JSONValueTest, EqualUint64) {
  auto jv = json::value();
  jv.emplace_uint64() = 10;
  GTEST_ASSERT_NE(jv, true);
  GTEST_ASSERT_NE(jv, -10);
  GTEST_ASSERT_EQ(jv, 10);
  GTEST_ASSERT_NE(jv, 10.0);
}

TEST (JSONValueTest, EqualDouble) {
  auto jv = json::value();
  jv.emplace_double() = 10.0;
  GTEST_ASSERT_NE(jv, true);
  GTEST_ASSERT_NE(jv, -10);
  GTEST_ASSERT_NE(jv, 10);
  GTEST_ASSERT_EQ(jv, 10.0);
}
}