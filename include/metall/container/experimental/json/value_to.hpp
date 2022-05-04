// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENTAL_JSON_VALUE_TO_HPP
#define METALL_CONTAINER_EXPERIMENTAL_JSON_VALUE_TO_HPP

#include <boost/json/src.hpp>

#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json::jsndtl {

namespace {
namespace mj = metall::container::experimental::json;
namespace bj = boost::json;
}

template <typename allocator_type>
inline void value_to_impl_helper(const mj::value<allocator_type> &input_value, bj::value *out_bj_value) {
  if (input_value.is_bool()) {
    *out_bj_value = input_value.as_bool();
  } else if (input_value.is_int64()) {
    *out_bj_value = input_value.as_int64();
  } else if (input_value.is_uint64()) {
    *out_bj_value = input_value.as_uint64();
  } else if (input_value.is_double()) {
    *out_bj_value = input_value.as_double();
  } else if (input_value.is_string()) {
    *out_bj_value = input_value.as_string().c_str();
  } else if (input_value.is_array()) {
    bj::array bj_array;
    for (const auto &elem : input_value.as_array()) {
      bj_array.emplace_back(mj::value_to<bj::value>(elem));
    }
    *out_bj_value = bj_array;
  } else if (input_value.is_object()) {
    bj::object bj_object;
    for (const auto &elem : input_value.as_object()) {
      bj_object[elem.key().data()] = mj::value_to<bj::value>(elem.value());
    }
    *out_bj_value = bj_object;
  } else if (input_value.is_null()) {
    out_bj_value->emplace_null();
  }
}

template <typename allocator_type>
inline bj::value value_to_impl(const mj::value<allocator_type> &input_value) {
  // TODO: support this syntax
  //  return bj::value_from(input_value);

  bj::value out_value;
  value_to_impl_helper(input_value, &out_value);
  return out_value;
}

} // namespace metall::container::experimental::json::jsndtl

namespace metall::container::experimental::json {

namespace {
namespace mj = metall::container::experimental::json;
}

/// \brief Convert a JSON value to another data type.
/// \tparam T The type to convert.
/// \tparam allocator_type The allocator type of the input.
/// \param value An input JSON value.
/// \return An instance of T that contains the data in 'value'.
template <typename T, typename allocator_type>
T value_to(const mj::value<allocator_type> &value) {
  return jsndtl::value_to_impl(value);
}

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENTAL_JSON_VALUE_TO_HPP
