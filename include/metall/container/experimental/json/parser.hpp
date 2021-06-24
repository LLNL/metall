// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_PARSER_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_PARSER_HPP

#include <iostream>
#include <string_view>

#include <boost/json/src.hpp>

#include <metall/container/experimental/json/json_fwd.hpp>
#include <metall/container/experimental/json/value.hpp>

namespace metall::container::experimental::json {

namespace {
namespace bj = boost::json;
}

/// \brief Parses a JSON stored in an object of boost::json::value.
/// \tparam allocator_type An allocator type.
/// \param input_value An input boost::json::value object.
/// \param out_value A pointer to store parsed JSON.
/// \return Returns true on success; otherwise, false.
template <typename allocator_type>
inline bool parse(const bj::value &input_value, value<allocator_type> *out_value) {
  if (!out_value)return false;

  if (input_value.is_null()) {
    out_value->emplace_null();
  } else if (input_value.is_bool()) {
    out_value->emplace_bool() = input_value.as_bool();
  } else if (input_value.is_int64()) {
    out_value->emplace_int64() = input_value.as_int64();
  } else if (input_value.is_uint64()) {
    out_value->emplace_uint64() = input_value.as_uint64();
  } else if (input_value.is_double()) {
    out_value->emplace_double() = input_value.as_double();
  } else if (input_value.is_string()) {
    out_value->emplace_string() = input_value.as_string().c_str();
  } else if (input_value.is_array()) {
    auto &out_array = out_value->emplace_array();
    for (const auto &item : input_value.as_array()) {
      out_array.resize(out_array.size() + 1);
      parse(item, &out_array[out_array.size() - 1]);
    }
  } else if (input_value.is_object()) {
    auto &out_obj = out_value->emplace_object();
    for (const auto &pair : input_value.as_object()) {
      auto &val = out_obj[pair.key_c_str()];
      parse(pair.value(), &val);
    }
  } else {
    std::cerr << "Unexpected Boost::json::kind" << std::endl;
    std::abort();
  }

  return true;
}

/// \brief Parses a JSON represented as a string.
/// \tparam allocator_type An allocator type.
/// \param input_json_string An input JSON string.
/// \param out_value A pointer to store parsed JSON.
/// \return Returns true on success; otherwise, false.
template <typename allocator_type>
inline bool parse(const std::string_view &input_json_string, value<allocator_type> *out_value) {
  bj::error_code ec;
  auto input_json_value = bj::parse(input_json_string.data(), ec);
  if (ec) {
    std::cerr << "Parsing failed: " << ec.message() << std::endl;
    return false;
  }

  return parse(input_json_value, out_value);
}

}

#endif //METALL_CONTAINER_EXPERIMENT_JSON_PARSER_HPP
