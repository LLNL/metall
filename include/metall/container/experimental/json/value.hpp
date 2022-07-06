// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP

#include <memory>
#include <utility>
#include <string_view>
#include <variant>

#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

namespace {
namespace mc = metall::container;
}

namespace jsndtl {

/// \brief Provides 'equal' calculation for other value types that have the same interface as the value class.
template <typename allocator_type, typename other_value_type>
inline bool general_value_equal(const value<allocator_type> &value, const other_value_type &other_value) noexcept {
  if (other_value.is_null()) {
    return value.is_null();
  } else if (other_value.is_bool()) {
    return value.is_bool() && value.as_bool() == other_value.as_bool();
  } else if (other_value.is_int64()) {
    if (value.is_int64()) {
      return value.as_int64() == other_value.as_int64();
    }
    if (value.is_uint64()) {
      return (other_value.as_int64() < 0) ? false : value.as_uint64()
          == static_cast<std::uint64_t>(other_value.as_int64());
    }
    return false;
  } else if (other_value.is_uint64()) {
    if (value.is_uint64()) {
      return value.as_uint64() == other_value.as_uint64();
    }
    if (value.is_int64()) {
      return (value.as_int64() < 0) ? false : static_cast<std::uint64_t>(value.as_int64()) == other_value.as_uint64();
    }
    return false;
  } else if (other_value.is_double()) {
    return value.is_double() && value.as_double() == other_value.as_double();
  } else if (other_value.is_object()) {
    return value.is_object() && (value.as_object() == other_value.as_object());
  } else if (other_value.is_array()) {
    return value.is_array() && (value.as_array() == other_value.as_array());
  } else if (other_value.is_string()) {
    return value.is_string() && (value.as_string() == other_value.as_string());
  }

  assert(false);
  return false;
}
} // namespace metall::container::experimental::json::jsndtl

/// \brief JSON value.
/// A container that holds a single bool, int64, uint64, double, JSON string, JSON array, or JSON object.
template <typename _allocator_type = std::allocator<std::byte>>
class value {
 public:
  using allocator_type = _allocator_type;
  using string_type = string<char,
                             std::char_traits<char>,
                             typename std::allocator_traits<allocator_type>::template rebind_alloc<char>>;
  using object_type = object<_allocator_type>;
  using array_type = array<_allocator_type>;

 private:
  using internal_data_type = std::variant<null_type, bool, std::int64_t, std::uint64_t, double,
                                          object_type, array_type, string_type>;

 public:
  /// \brief Constructor.
  value() {}

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit value(const allocator_type &alloc)
      : m_allocator(alloc) {}

  /// \brief Copy constructor
  value(const value &) = default;

  ~value() noexcept {
    priv_reset();
  }

  /// \brief Allocator-extended copy constructor
  value(const value &other, const allocator_type &alloc)
      : m_allocator(alloc) {
    if (other.is_object()) {
      emplace_object() = other.as_object();
    } else if (other.is_array()) {
      emplace_array() = other.as_array();
    } else if (other.is_string()) {
      emplace_string() = other.as_string();
    } else {
      m_data = other.m_data;
    }
  }

  /// \brief Move constructor
  value(value &&other) noexcept
      : m_data(std::move(other.m_data)),
        m_allocator(std::move(other.m_allocator)) {
    other.priv_reset();
  }

  /// \brief Allocator-extended move constructor
  value(value &&other, const allocator_type &alloc) noexcept
      : m_allocator(alloc) {
    if (other.is_object()) {
      emplace_object() = std::move(other.as_object());
    } else if (other.is_array()) {
      emplace_array() = std::move(other.as_array());
    } else if (other.is_string()) {
      emplace_string() = std::move(other.as_string());
    } else {
      m_data = std::move(other.m_data);
    }
    other.priv_reset();
  }

  /// \brief Copy assignment operator
  value &operator=(const value &) = default;

  /// \brief Move assignment operator
  value &operator=(value &&other) noexcept {
    if (other.is_object()) {
      emplace_object() = std::move(other.as_object());
    } else if (other.is_array()) {
      emplace_array() = std::move(other.as_array());
    } else if (other.is_string()) {
      emplace_string() = std::move(other.as_string());
    } else {
      m_data = std::move(other.m_data);
    }
    other.priv_reset();
    return *this;
  }

  /// \brief Assign a bool value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const bool b) {
    emplace_bool() = b;
    return *this;
  }

  /// \brief Assign a char value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const signed char i) {
    return operator=(static_cast<long long>(i));
  }

  /// \brief Assign a short value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const short i) {
    return operator=(static_cast<long long>(i));
  }

  /// \brief Assign an int value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const int i) {
    return operator=(static_cast<long long>(i));
  }

  /// \brief Assign a long value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const long i) {
    return operator=(static_cast<long long>(i));
  }

  /// \brief Assign a long long value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const long long i) {
    emplace_int64() = i;
    return *this;
  }

  /// \brief Assign an unsigned char value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const unsigned char u) {
    return operator=(static_cast<unsigned long long>(u));
  }

  /// \brief Assign an unsigned short value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const unsigned short u) {
    return operator=(static_cast<unsigned long long>(u));
  }

  /// \brief Assign an unsigned int value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const unsigned int u) {
    return operator=(static_cast<unsigned long long>(u));
  }

  /// \brief Assign an unsigned long value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const unsigned long u) {
    return operator=(static_cast<unsigned long long>(u));
  }

  /// \brief Assign an unsigned long long value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const unsigned long long u) {
    emplace_uint64() = u;
    return *this;
  }

  /// \brief Assign a null value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(std::nullptr_t) {
    emplace_null();
    return *this;
  }

  /// \brief Assign a double value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const double d) {
    emplace_double() = d;
    return *this;
  }

  /// \brief Assign a std::string_view value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(std::string_view s) {
    emplace_string() = s.data();
    return *this;
  }

  /// \brief Assign a const char* value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const char *const s) {
    emplace_string() = s;
    return *this;
  }

  /// \brief Assign a string_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const string_type &s) {
    emplace_string() = s;
    return *this;
  }

  // value &operator=(std::initializer_list<value_ref> init);

  /// \brief Assign a string_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(string_type &&s) {
    emplace_string() = std::move(s);
    return *this;
  }

  /// \brief Assign an array_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const array_type &arr) {
    emplace_array() = arr;
    return *this;
  }

  /// \brief Assign an array_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(array_type &&arr) {
    emplace_array() = std::move(arr);
    return *this;
  }

  /// \brief Assign an object_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(const object_type &obj) {
    emplace_object() = obj;
    return *this;
  }

  /// \brief Assign an object_type value.
  /// Allocates a memory storage or destroy the old content, if necessary.
  value &operator=(object_type &&obj) {
    emplace_object() = std::move(obj);
    return *this;
  }

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

  /// \brief Return an allocator object.
  allocator_type get_allocator() const noexcept {
    return m_allocator;
  }

  /// \brief Equal operator.
  friend bool operator==(const value &lhs, const value &rhs) noexcept {
    return jsndtl::general_value_equal(lhs, rhs);;
  }

  /// \brief Return `true` if two values are not equal.
  /// Two values are equal when they are the
  /// same kind and their referenced values
  /// are equal, or when they are both integral
  /// types and their integral representations are equal.
  friend bool operator!=(const value &lhs, const value &rhs) noexcept {
    return !(lhs == rhs);
  }

 private:

  bool priv_reset() {
    m_data.template emplace<null_type>();
    return true;
  }

  internal_data_type m_data{null_type{}};
  allocator_type m_allocator{allocator_type{}};
};

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_VALUE_HPP
