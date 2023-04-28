// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_SERIALIZE_HPP
#define METALL_JSON_SERIALIZE_HPP

#include <iostream>

#include <metall/container/string.hpp>
#include <metall/json/json_fwd.hpp>

namespace metall::json {

namespace {
namespace mc = metall::container;
namespace bj = boost::json;
}  // namespace

template <typename allocator_type>
std::string serialize(const value<allocator_type> &input) {
  return bj::serialize(value_to<bj::value>(input));
}

template <typename allocator_type>
std::string serialize(const object<allocator_type> &input) {
  bj::object object;
  for (const auto &elem : input) {
    object[elem.key().data()] = value_to<bj::value>(elem.value());
  }
  return bj::serialize(object);
}

template <typename allocator_type>
std::string serialize(const array<allocator_type> &input) {
  bj::array array;
  for (const auto &elem : input) {
    array.emplace_back(value_to<bj::value>(elem));
  }
  return bj::serialize(array);
}

template <typename char_type, typename traits, typename allocator_type>
std::string serialize(
    const basic_string<char_type, traits, allocator_type> &input) {
  return input.data();
}

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const value<allocator_type> &val) {
  os << serialize(val);
  return os;
}

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const object<allocator_type> &obj) {
  os << serialize(obj);
  return os;
}

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const array<allocator_type> &arr) {
  os << serialize(arr);
  return os;
}

}  // namespace metall::json

#endif  // METALL_JSON_SERIALIZE_HPP
