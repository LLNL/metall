// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <string_view>
#include <variant>

#include <boost/json/src.hpp>

#include <metall/container/experiment/json/json_fwd.hpp>
#include <metall/container/experiment/json/string.hpp>
#include <metall/container/experiment/json/parser.hpp>

namespace metall::container::experiment::json {

namespace {
namespace bj = boost::json;
}

/// \brief JSON null.
using null_type = std::monostate;

/// \brief JSON value.
/// A single, bool, int64, uint64, double, string, array, or object.
template <typename _allocator_type = std::allocator<std::byte>>
class value {
 public:
  using allocator_type = _allocator_type;
  using string_type = string<typename std::allocator_traits<allocator_type>::template rebind_alloc<char>>;
  using object_type = object<_allocator_type>;
  using array_type = array<_allocator_type>;

 private:
  using internal_data_type = std::variant<null_type, bool, std::int64_t, std::uint64_t, double,
                                          object_type, array_type, string_type>;

 public:
  explicit value(const allocator_type &alloc = allocator_type())
      : m_allocator(alloc) {}

  explicit value(const std::string_view &input_json_string, const allocator_type &alloc = allocator_type())
      : m_allocator(alloc) {
    parse(input_json_string, this);
  }

  explicit value(const bj::value &input_json_value, const allocator_type &alloc = allocator_type())
      : m_allocator(alloc) {
    parse(input_json_value, this);
  }

  /// \brief Copy constructor
  value(const value &) = default;

  /// \brief Allocator-extended copy constructor
  /// This will be used by scoped-allocator
  value(const value &other, const allocator_type &alloc)
      : m_data(other.m_data),
        m_allocator(alloc) {}

  /// \brief Move constructor
  value(value &&) noexcept = default;

  /// \brief Allocator-extended move constructor
  /// This will be used by scoped-allocator
  value(value &&other, const allocator_type &alloc) noexcept
      : m_data(std::move(other.m_data)),
        m_allocator(alloc) {}

  value &operator=(const value &) = default;
  value &operator=(value &&) noexcept = default;

  /// \brief Set a null.
  /// The old content is destroyed.
  void emplace_null() {
    priv_reset();
  }

  /// \brief Set a bool and return a reference.
  /// The old content is destroyed.
  bool &emplace_bool() {
    priv_reset();
    return m_data.template emplace<bool>();
  }

  /// \brief Set a int64 and return a reference.
  /// The old content is destroyed.
  std::int64_t &emplace_int64() {
    priv_reset();
    return m_data.template emplace<std::int64_t>();
  }

  /// \brief Set a uint64 and return a reference.
  /// The old content is destroyed.
  std::uint64_t &emplace_uint64() {
    priv_reset();
    return m_data.template emplace<std::uint64_t>();
  }

  /// \brief Set a double and return a reference.
  /// The old content is destroyed.
  double &emplace_double() {
    priv_reset();
    return m_data.template emplace<double>();
  }

  /// \brief Set an empty string and return a reference.
  /// The old content is destroyed.
  string_type &emplace_string() {
    priv_reset();
    return m_data.template emplace<string_type>(m_allocator);
  }

  /// \brief Set an empty array and return a reference.
  /// The old content is destroyed.
  array_type &emplace_array() {
    priv_reset();
    return m_data.template emplace<array_type>(m_allocator);
  }

  /// \brief Set an empty object and return a reference.
  /// The old content is destroyed.
  object_type &emplace_object() {
    priv_reset();
    return m_data.template emplace<object_type>(m_allocator);
  }

  /// \brief Return a reference to the underlying bool, or throw an exception.
  bool &as_bool() {
    return std::get<bool>(m_data);
  }

  /// \brief Return a const reference to the underlying bool, or throw an exception.
  const bool &as_bool() const {
    return std::get<bool>(m_data);
  }

  /// \brief Return a reference to the underlying std::int64_t, or throw an exception.
  std::int64_t &as_int64() {
    return std::get<std::int64_t>(m_data);
  }

  /// \brief Return a const reference to the underlying std::int64_t, or throw an exception.
  const std::int64_t &as_int64() const {
    return std::get<std::int64_t>(m_data);
  }

  /// \brief Return a reference to the underlying std::uint64_t, or throw an exception.
  std::uint64_t &as_uint64() {
    return std::get<std::uint64_t>(m_data);
  }

  /// \brief Return a const reference to the underlying std::uint64_t, or throw an exception.
  const std::uint64_t &as_uint64() const {
    return std::get<std::uint64_t>(m_data);
  }

  /// \brief Return a reference to the underlying double, or throw an exception.
  double &as_double() {
    return std::get<double>(m_data);
  }

  /// \brief Return a const reference to the underlying double, or throw an exception.
  const double &as_double() const {
    return std::get<double>(m_data);
  }

  /// \brief Return a reference to the underlying string, or throw an exception.
  string_type &as_string() {
    return std::get<string_type>(m_data);
  }

  /// \brief Return a const reference to the underlying string, or throw an exception.
  const string_type &as_string() const {
    return std::get<string_type>(m_data);
  }

  /// \brief Return a reference to the underlying array, or throw an exception.
  array_type &as_array() {
    return std::get<array_type>(m_data);
  }

  /// \brief Return a const reference to the underlying array, or throw an exception.
  const array_type &as_array() const {
    return std::get<array_type>(m_data);
  }

  /// \brief Return a reference to the underlying object, or throw an exception.
  object_type &as_object() {
    return std::get<object_type>(m_data);
  }

  /// \brief Return a const reference to the underlying object, or throw an exception.
  const object_type &as_object() const {
    return std::get<object_type>(m_data);
  }

  /// \brief Return true if this is a null.
  bool is_null() const noexcept {
    return std::holds_alternative<null_type>(m_data);
  }

  /// \brief Return true if this is a bool.
  bool is_bool() const noexcept {
    return std::holds_alternative<bool>(m_data);
  }

  /// \brief Return true if this is a int64.
  bool is_int64() const noexcept {
    return std::holds_alternative<int64_t>(m_data);
  }

  /// \brief Return true if this is a uint64.
  bool is_uint64() const noexcept {
    return std::holds_alternative<uint64_t>(m_data);
  }

  /// \brief Return true if this is a double.
  bool is_double() const noexcept {
    return std::holds_alternative<double>(m_data);
  }

  /// \brief Return true if this is a string.
  bool is_string() const noexcept {
    return std::holds_alternative<string_type>(m_data);
  }

  /// \brief Return true if this is an array.
  bool is_array() const noexcept {
    return std::holds_alternative<array_type>(m_data);
  }

  /// \brief Return true if this is a object.
  bool is_object() const noexcept {
    return std::holds_alternative<object_type>(m_data);
  }

  allocator_type get_allocator() const noexcept {
    return m_allocator;
  }

 private:
  bool priv_reset() {
    std::visit([this](const auto &arg) {
      using T = std::decay_t<decltype(arg)>;
      arg.~T();
    }, m_data);
    m_data.template emplace<null_type>();
    return true;
  }

  internal_data_type m_data{null_type{}};
  allocator_type m_allocator;
};

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const json::value<allocator_type> &val) {
  if (val.is_null()) {
    os << "null";
  } else if (val.is_bool()) {
    os << std::boolalpha << val.as_bool() << "";
    os << std::noboolalpha;
  } else if (val.is_int64()) {
    os << val.as_int64() << "";
  } else if (val.is_uint64()) {
    os << val.as_uint64() << "";
  } else if (val.is_double()) {
    os << val.as_double() << "";
  } else if (val.is_string()) {
    os << "\"" << val.as_string() << "\"";
  } else if (val.is_array()) {
    os << val.as_array() << "";
  } else if (val.is_object()) {
    os << val.as_object() << "";
  } else {
    os << "Invalid type\n";
  }

  return os;
}

}

#endif //METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP
