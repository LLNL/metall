// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_VALUE_FROM_HPP
#define METALL_JSON_VALUE_FROM_HPP

#include <metall/json/json_fwd.hpp>

namespace metall::json::jsndtl {

namespace {
namespace bj = boost::json;
}

template <typename allocator_type>
inline value<allocator_type> value_from_impl(
    const bj::value &input_bj_value,
    allocator_type allocator = allocator_type()) {
  value<allocator_type> out_value(allocator);

  if (input_bj_value.is_null()) {
    out_value.emplace_null();
  } else if (input_bj_value.is_bool()) {
    out_value = input_bj_value.as_bool();
  } else if (input_bj_value.is_int64()) {
    out_value = input_bj_value.as_int64();
  } else if (input_bj_value.is_uint64()) {
    out_value = input_bj_value.as_uint64();
  } else if (input_bj_value.is_double()) {
    out_value = input_bj_value.as_double();
  } else if (input_bj_value.is_string()) {
    const auto& str = input_bj_value.as_string();
    out_value.emplace_string().assign(str.c_str(), str.size());
  } else if (input_bj_value.is_array()) {
    auto &out_array = out_value.emplace_array();
    for (const auto &item : input_bj_value.as_array()) {
      out_array.resize(out_array.size() + 1);
      out_array[out_array.size() - 1] = value_from_impl(item, allocator);
    }
  } else if (input_bj_value.is_object()) {
    auto &out_obj = out_value.emplace_object();
    for (const auto &pair : input_bj_value.as_object()) {
      out_obj[pair.key_c_str()] = value_from_impl(pair.value(), allocator);
    }
  }

  return out_value;
}

template <typename allocator_type>
inline value<allocator_type> value_from_impl(
    bj::value &&input_bj_value, allocator_type allocator = allocator_type()) {
  bj::value tmp_input_bj_value(std::move(input_bj_value));
  return value_from_impl(tmp_input_bj_value, allocator);
}

}  // namespace metall::json::jsndtl

namespace metall::json {

/// \brief Convert an input data and construct a JSON value.
/// \tparam T The type of an input data.
/// \tparam allocator_type An allocator type to allocate a return value.
/// \param input_data Input data.
/// \param allocator An allocator object.
/// \return Returns a constructed JSON value.
#ifdef DOXYGEN_SKIP
template <typename T, typename allocator_type = std::allocator<std::byte>>
inline value<allocator_type> value_from(
    T &&input_data, const allocator_type &allocator = allocator_type())
#else
template <typename T, typename allocator_type>
inline value<allocator_type> value_from(T &&input_data,
                                        const allocator_type &allocator)
#endif
{
  return jsndtl::value_from_impl(std::forward<T>(input_data), allocator);
}

}  // namespace metall::json

#endif  // METALL_JSON_VALUE_FROM_HPP
