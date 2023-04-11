// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_VALUE_HPP
#define METALL_JSON_VALUE_HPP

#include <memory>
#include <utility>
#include <string_view>
#include <variant>
#include <type_traits>

#include <metall/json/json_fwd.hpp>

namespace metall::json {

namespace {
namespace mc = metall::container;
}

namespace jsndtl {

/// \brief Provides 'equal' calculation for other value types that have the same
/// interface as the value class.
template <typename allocator_type, typename other_value_type>
inline bool general_value_equal(const value<allocator_type> &value,
                                const other_value_type &other_value) noexcept {
  if (other_value.is_null()) {
    return value.is_null();
  } else if (other_value.is_bool()) {
    return value.is_bool() && value.as_bool() == other_value.as_bool();
  } else if (other_value.is_int64()) {
    if (value.is_int64()) {
      return value.as_int64() == other_value.as_int64();
    }
    if (value.is_uint64()) {
      return (other_value.as_int64() < 0)
                 ? false
                 : value.as_uint64() ==
                       static_cast<std::uint64_t>(other_value.as_int64());
    }
    return false;
  } else if (other_value.is_uint64()) {
    if (value.is_uint64()) {
      return value.as_uint64() == other_value.as_uint64();
    }
    if (value.is_int64()) {
      return (value.as_int64() < 0)
                 ? false
                 : static_cast<std::uint64_t>(value.as_int64()) ==
                       other_value.as_uint64();
    }
    return false;
  } else if (other_value.is_double()) {
    return value.is_double() && value.as_double() == other_value.as_double();
  } else if (other_value.is_object()) {
    return value.is_object() && (value.as_object() == other_value.as_object());
  } else if (other_value.is_array()) {
    return value.is_array() && (value.as_array() == other_value.as_array());
  } else if (other_value.is_string()) {
    if (!value.is_string()) return false;
    const auto &str = value.as_string();
    const auto &other_srt = other_value.as_string();
    return str.compare(other_srt.c_str()) == 0;
  }

  assert(false);
  return false;
}
}  // namespace jsndtl

/// \brief JSON value.
/// A container that holds a single bool, int64, uint64, double, JSON string,
/// JSON array, or JSON object.
#ifdef DOXYGEN_SKIP
template <typename Alloc = std::allocator<std::byte>>
#else
template <typename Alloc>
#endif
class value {
 public:
  using allocator_type = Alloc;
  using string_type =
      basic_string<char, std::char_traits<char>,
                   typename std::allocator_traits<
                       allocator_type>::template rebind_alloc<char>>;
  using object_type = object<Alloc>;
  using array_type = array<Alloc>;

 private:
  using internal_data_type =
      std::variant<null_type, bool, std::int64_t, std::uint64_t, double,
                   object_type, array_type, string_type>;

 public:
  /// \brief Constructor.
  value() { priv_reset(); }

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit value(const allocator_type &alloc) : m_allocator(alloc) {
    priv_reset();
  }

  /// \brief Copy constructor
  value(const value &other)
      : m_allocator(
            std::allocator_traits<allocator_type>::
                select_on_container_copy_construction(other.get_allocator())),
        m_data(other.m_data) {}

  /// \brief Allocator-extended copy constructor
  value(const value &other, const allocator_type &alloc) : m_allocator(alloc) {
    if (other.is_object()) {
      m_data.template emplace<object_type>(other.as_object(), m_allocator);
    } else if (other.is_array()) {
      m_data.template emplace<array_type>(other.as_array(), m_allocator);
    } else if (other.is_string()) {
      m_data.template emplace<string_type>(other.as_string(), m_allocator);
    } else {
      m_data = other.m_data;
    }
  }

  /// \brief Move constructor
  value(value &&other) noexcept
      : m_allocator(std::move(other.m_allocator)),
        m_data(std::move(other.m_data)) {
    other.priv_reset();
  }

  /// \brief Allocator-extended move constructor
  value(value &&other, const allocator_type &alloc) noexcept
      : m_allocator(alloc) {
    if (other.is_object()) {
      m_data.template emplace<object_type>(std::move(other.as_object()),
                                           m_allocator);
    } else if (other.is_array()) {
      m_data.template emplace<array_type>(std::move(other.as_array()),
                                          m_allocator);
    } else if (other.is_string()) {
      m_data.template emplace<string_type>(std::move(other.as_string()),
                                           m_allocator);
    } else {
      m_data = std::move(other.m_data);
    }
    other.priv_reset();
  }

  /// \brief Destructor
  ~value() noexcept { priv_reset(); }

  /// \brief Copy assignment operator
  value &operator=(const value &other) {
    if (this == &other) return *this;

    if constexpr (std::is_same_v<
                      typename std::allocator_traits<allocator_type>::
                          propagate_on_container_copy_assignment,
                      std::true_type>) {
      m_allocator = other.m_allocator;
    }

    // Cannot do `m_data = other.m_data`
    // because std::variant calls the allocator-extended copy constructor of the
    // holding data, which ignores propagate_on_container_copy_assignment value.
    if (other.is_object()) {
      emplace_object() = other.as_object();
    } else if (other.is_array()) {
      emplace_array() = other.as_array();
    } else if (other.is_string()) {
      emplace_string() = other.as_string();
    } else {
      m_data = other.m_data;
    }

    return *this;
  }

  /// \brief Move assignment operator
  value &operator=(value &&other) noexcept {
    if (this == &other) return *this;

    if constexpr (std::is_same_v<
                      typename std::allocator_traits<allocator_type>::
                          propagate_on_container_move_assignment,
                      std::true_type>) {
      m_allocator = std::move(other.m_allocator);
    }

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

  /// \brief Swap contents.
  void swap(value &other) noexcept {
    using std::swap;
    if constexpr (std::is_same_v<
                      typename std::allocator_traits<
                          allocator_type>::propagate_on_container_swap,
                      std::true_type>) {
      swap(m_allocator, other.m_allocator);
    } else {
      // This is an undefined behavior in the C++ standard.
      assert(get_allocator() == other.m_allocator);
    }

    swap(m_data, other.m_data);
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
  value &operator=(const int i) { return operator=(static_cast<long long>(i)); }

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
  void emplace_null() { priv_reset(); }

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
  bool &as_bool() { return std::get<bool>(m_data); }

  /// \brief Return a const reference to the underlying bool, or throw an
  /// exception.
  const bool &as_bool() const { return std::get<bool>(m_data); }

  /// \brief Return a reference to the underlying std::int64_t, or throw an
  /// exception.
  std::int64_t &as_int64() { return std::get<std::int64_t>(m_data); }

  /// \brief Return a const reference to the underlying std::int64_t, or throw
  /// an exception.
  const std::int64_t &as_int64() const {
    return std::get<std::int64_t>(m_data);
  }

  /// \brief Return a reference to the underlying std::uint64_t, or throw an
  /// exception.
  std::uint64_t &as_uint64() { return std::get<std::uint64_t>(m_data); }

  /// \brief Return a const reference to the underlying std::uint64_t, or throw
  /// an exception.
  const std::uint64_t &as_uint64() const {
    return std::get<std::uint64_t>(m_data);
  }

  /// \brief Return a reference to the underlying double, or throw an exception.
  double &as_double() { return std::get<double>(m_data); }

  /// \brief Return a const reference to the underlying double, or throw an
  /// exception.
  const double &as_double() const { return std::get<double>(m_data); }

  /// \brief Return a reference to the underlying string, or throw an exception.
  string_type &as_string() { return std::get<string_type>(m_data); }

  /// \brief Return a const reference to the underlying string, or throw an
  /// exception.
  const string_type &as_string() const { return std::get<string_type>(m_data); }

  /// \brief Return a reference to the underlying array, or throw an exception.
  array_type &as_array() { return std::get<array_type>(m_data); }

  /// \brief Return a const reference to the underlying array, or throw an
  /// exception.
  const array_type &as_array() const { return std::get<array_type>(m_data); }

  /// \brief Return a reference to the underlying object, or throw an exception.
  object_type &as_object() { return std::get<object_type>(m_data); }

  /// \brief Return a const reference to the underlying object, or throw an
  /// exception.
  const object_type &as_object() const { return std::get<object_type>(m_data); }

  /// \brief Return true if this is a null.
  bool is_null() const noexcept {
    return std::holds_alternative<null_type>(m_data);
  }

  /// \brief Return true if this is a bool.
  bool is_bool() const noexcept { return std::holds_alternative<bool>(m_data); }

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
  allocator_type get_allocator() const noexcept { return m_allocator; }

  /// \brief Equal operator.
  friend bool operator==(const value &lhs, const value &rhs) noexcept {
    return jsndtl::general_value_equal(lhs, rhs);
    ;
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

  allocator_type m_allocator{allocator_type{}};
  internal_data_type m_data{null_type{}};
};

/// \brief Swap value instances.
template <typename allocator_type>
inline void swap(value<allocator_type> &lhd,
                 value<allocator_type> &rhd) noexcept {
  lhd.swap(rhd);
}

}  // namespace metall::json

#endif  // METALL_JSON_VALUE_HPP
