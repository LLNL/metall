// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_PRETTY_PRINT_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_PRETTY_PRINT_HPP

#include <iostream>

#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json::jsndtl {
template <typename allocator_type, int indent_size>
inline void pretty_print_impl(std::ostream &os, const value<allocator_type> &jv, const std::string &indent) {
  if (jv.is_bool()) {
    os << std::boolalpha << jv.as_bool();
  } else if (jv.is_int64()) {
    os << jv.as_int64();
  } else if (jv.is_uint64()) {
    os << jv.as_uint64();
  } else if (jv.is_double()) {
    os << jv.as_double();
  } else if (jv.is_string()) {
    os << json::serialize(jv.as_string());
  } else if (jv.is_array()) {
    os << "[\n";
    const auto &arr = jv.as_array();
    std::string new_indent = indent;
    new_indent.append(indent_size, ' ');
    for (std::size_t i = 0; i < arr.size(); ++i) {
      os << new_indent;
      pretty_print_impl<allocator_type, indent_size>(os, arr[i], new_indent);
      if (i < arr.size() - 1) {
        os << ",\n";
      }
    }
    os << "\n" << indent << "]";
  } else if (jv.is_object()) {
    os << "{\n";
    const auto &obj = jv.as_object();
    std::string new_indent = indent;
    new_indent.append(indent_size, ' ');
    for (auto it = obj.begin();;) {
      os << new_indent << it->key() << " : ";
      pretty_print_impl<allocator_type, indent_size>(os, it->value(), new_indent);
      if (++it == obj.end()) {
        break;
      }
      os << ",\n";
    }
    os << "\n" << indent << "}";

  } else if (jv.is_null()) {
    os << "null";
  }
}

} // namespace metall::container::experimental::json::jsndtl

namespace metall::container::experimental::json {

/// \brief Pretty-prints a JSON value.
/// \tparam allocator_type An allocator type used in the value.
/// \tparam indent_size The size of the indent when going to a lower layer.
/// \param os An output stream object.
/// \param jv A JSON value to print.
template <typename allocator_type, int indent_size = 2>
inline void pretty_print(std::ostream &os, const value<allocator_type> &jv) {
  std::string indent;
  jsndtl::pretty_print_impl<allocator_type, indent_size>(os, jv, indent);
  os << std::endl;
}

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_PRETTY_PRINT_HPP
