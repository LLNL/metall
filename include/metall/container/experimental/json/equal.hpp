// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_EQUAL_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_EQUAL_HPP

#include <boost/json/src.hpp>

#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

namespace {
namespace bj = boost::json;
}

template <typename char_type, typename char_traits, typename allocator_type>
inline bool operator==(const key_value_pair<char_type, char_traits, allocator_type> &key_value,
                       const bj::key_value_pair &bj_key_value) {
  return jsndtl::general_key_value_pair_equal(key_value, bj_key_value);
}

template <typename char_type, typename char_traits, typename allocator_type>
inline bool operator==(const bj::key_value_pair &bj_key_value,
                       const key_value_pair<char_type, char_traits, allocator_type> &key_value) {
  return key_value == bj_key_value;
}

template <typename char_type, typename char_traits, typename allocator_type>
inline bool operator!=(const key_value_pair<char_type, char_traits, allocator_type> &key_value,
                       const bj::key_value_pair &bj_key_value) {
  return !(key_value == bj_key_value);
}

template <typename char_type, typename char_traits, typename allocator_type>
inline bool operator!=(const bj::key_value_pair &bj_key_value,
                       const key_value_pair<char_type, char_traits, allocator_type> &key_value) {
  return key_value != bj_key_value;
}

template <typename allocator_type>
inline bool operator==(const value<allocator_type> &value, const bj::value &bj_value) {
  return jsndtl::general_value_equal(value, bj_value);
}

template <typename allocator_type>
inline bool operator==(const bj::value &bj_value, const value<allocator_type> &value) {
  return value == bj_value;
}

template <typename allocator_type>
inline bool operator!=(const value<allocator_type> &value, const bj::value &bj_value) {
  return !(value == bj_value);
}

template <typename allocator_type>
inline bool operator!=(const bj::value &bj_value, const value<allocator_type> &value) {
  return value != bj_value;
}

template <typename allocator_type>
inline bool operator==(const array<allocator_type> &array, const bj::array &bj_array) {
  return jsndtl::general_array_equal(array, bj_array);
}

template <typename allocator_type>
inline bool operator==(const bj::array &bj_array, const array<allocator_type> &array) {
  return array == bj_array;
}

template <typename allocator_type>
inline bool operator!=(const array<allocator_type> &array, const bj::array &bj_array) {
  return !(array == bj_array);
}

template <typename allocator_type>
inline bool operator!=(const bj::array &bj_array, const array<allocator_type> &array) {
  return array != bj_array;
}

template <typename allocator_type>
inline bool operator==(const compact_object<allocator_type> &object, const bj::object &bj_object) {
  return jsndtl::general_compact_object_equal(object, bj_object);
}

template <typename allocator_type>
inline bool operator==(const bj::object &bj_object, const compact_object<allocator_type> &object) {
  return object == bj_object;
}

template <typename allocator_type>
inline bool operator!=(const compact_object<allocator_type> &object, const bj::object &bj_object) {
  return !(object == bj_object);
}

template <typename allocator_type>
inline bool operator!=(const bj::object &bj_object, const compact_object<allocator_type> &object) {
  return object != bj_object;
}

template <typename allocator_type>
inline bool operator==(const indexed_object<allocator_type> &object, const bj::object &bj_object) {
  return jsndtl::general_indexed_object_equal(object, bj_object);
}

template <typename allocator_type>
inline bool operator==(const bj::object &bj_object, const indexed_object<allocator_type> &object) {
  return object == bj_object;
}

template <typename allocator_type>
inline bool operator!=(const indexed_object<allocator_type> &object, const bj::object &bj_object) {
  return !(object == bj_object);
}

template <typename allocator_type>
inline bool operator!=(const bj::object &bj_object, const indexed_object<allocator_type> &object) {
  return object != bj_object;
}

template <typename char_t, typename traits, typename allocator>
inline bool operator==(const string<char_t, traits, allocator> &string, const bj::string &bj_string) {
  return jsndtl::general_string_equal(string, bj_string);
}

template <typename char_t, typename traits, typename allocator>
inline bool operator==(const bj::string &bj_string, const string<char_t, traits, allocator> &string) {
  return string == bj_string;
}

template <typename char_t, typename traits, typename allocator>
inline bool operator!=(const string<char_t, traits, allocator> &string, const bj::string &bj_string) {
  return !(string == bj_string);
}

template <typename char_t, typename traits, typename allocator>
inline bool operator!=(const bj::string &bj_string, const string<char_t, traits, allocator> &string) {
  return string != bj_string;
}
} // namespace metall::container::experimental::json
#endif //METALL_CONTAINER_EXPERIMENT_JSON_EQUAL_HPP
