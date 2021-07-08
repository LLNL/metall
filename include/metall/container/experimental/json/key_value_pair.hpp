// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP

#include <string_view>
#include <memory>
#include <string>

#include <metall/container/string.hpp>
#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

/// \brief A class for holding a pair of JSON string (as its key) and JSON value (as its value).
/// \tparam char_type A char type to store.
/// \tparam char_traits A chart traits.
/// \tparam _allocator_type An allocator type.
template <typename _char_type = char,
          typename _char_traits = std::char_traits<char>,
          typename _allocator_type = std::allocator<std::byte>>
class key_value_pair {
 private:
  using char_allocator_type = typename std::allocator_traits<_allocator_type>::template rebind_alloc<_char_type>;
  using internal_key_type = metall::container::basic_string<_char_type, _char_traits, char_allocator_type>;

 public:
  using char_type = _char_type;
  using char_traits = _char_traits;
  using allocator_type = _allocator_type;
  using key_type = std::basic_string_view<char_type, char_traits>;
  using value_type = metall::container::experimental::json::value<allocator_type>;

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  key_value_pair(key_type key,
                 const value_type &value,
                 const allocator_type &alloc = allocator_type())
      : m_key(key, alloc),
        m_value(value, alloc) {}

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  key_value_pair(key_type key,
                 value_type &&value,
                 const allocator_type &alloc = allocator_type())
      : m_key(key, alloc),
        m_value(std::move(value), alloc) {}

  /// \brief Copy constructor
  key_value_pair(const key_value_pair &other) = default;

  /// \brief Allocator-extended copy constructor
  /// This will be used by scoped-allocator
  key_value_pair(const key_value_pair &other, const allocator_type &alloc)
      : m_key(other.m_key, alloc),
        m_value(other.m_value, alloc) {}

  /// \brief Move constructor
  key_value_pair(key_value_pair &&other) noexcept = default;

  /// \brief Allocator-extended move constructor
  /// This will be used by scoped-allocator
  key_value_pair(key_value_pair &&other, const allocator_type &alloc) noexcept
      : m_key(std::move(other.m_key), alloc),
        m_value(std::move(other.m_value), alloc) {}

  /// \brief Copy assignment operator
  key_value_pair &operator=(const key_value_pair &other) = default;

  /// \brief Move assignment operator
  key_value_pair &operator=(key_value_pair &&other) noexcept = default;

  /// \brief Destructor
  ~key_value_pair() noexcept = default;

  /// \brief Returns the stored key.
  /// \return Returns the stored key.
  const key_type key() const noexcept {
    return m_key;
  }

  /// \brief Returns the stored key as const char*.
  /// \return Returns the stored key as const char*.
  const char *key_c_str() const noexcept {
    return m_key.c_str();
  }

  /// \brief References the stored JSON value.
  /// \return Returns a reference to the stored JSON value.
  value_type &value() noexcept {
    return m_value;
  }

  /// \brief References the stored JSON value.
  /// \return Returns a reference to the stored JSON value.
  const value_type &value() const noexcept {
    return m_value;
  }

 private:
  internal_key_type m_key;
  value_type m_value;
};

} // namespace json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP
