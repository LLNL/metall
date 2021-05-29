// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP

#include <string_view>
#include <memory>
#include <string>

#include <metall/offset_ptr.hpp>
#include <metall/container/experimental/json/json_fwd.hpp>
#include <metall/container/experimental/json/value.hpp>

namespace metall::container::experimental::json {

/// \brief A class for holding a pair of JSON string (as its key) and JSON value (as its value).
/// \tparam char_type A char type to store.
/// \tparam char_traits A chart traits.
/// \tparam _allocator_type An allocator type.
template <typename char_type = char,
          typename char_traits = std::char_traits<char_type>,
          typename _allocator_type = std::allocator<std::byte>>
class basic_key_value_pair {
 private:
  using char_allocator_type = typename std::allocator_traits<_allocator_type>::template rebind_alloc<char_type>;
  using char_pointer = typename std::allocator_traits<char_allocator_type>::pointer;

 public:
  using allocator_type = _allocator_type;
  using key_type = std::basic_string_view<char_type, char_traits>;
  using value_type = metall::container::experimental::json::value<allocator_type>;

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  basic_key_value_pair(key_type key,
                       const value_type &value,
                       const allocator_type &alloc = allocator_type())
      : m_value(value, alloc) {
    priv_allocate_key(key);
  }

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  basic_key_value_pair(key_type key,
                       value_type &&value,
                       const allocator_type &alloc = allocator_type())
      : m_value(std::move(value), alloc) {
    priv_allocate_key(key);
  }

  /// \brief Copy constructor
  basic_key_value_pair(const basic_key_value_pair &other)
      : m_value(other.m_value) {
    priv_allocate_key(other.key());
  }

  /// \brief Allocator-extended copy constructor
  /// This will be used by scoped-allocator
  basic_key_value_pair(const basic_key_value_pair &other, const allocator_type &alloc)
      : m_value(other.m_value, alloc) {
    priv_allocate_key(other.key());
  }

  /// \brief Move constructor
  basic_key_value_pair(basic_key_value_pair &&other) noexcept
      : m_key(std::move(other.m_key)),
        m_length(std::move(other.m_length)),
        m_value(std::move(other.m_value)) {
    other.m_key = nullptr;
    other.m_length = 0;
  }

  /// \brief Allocator-extended move constructor
  /// This will be used by scoped-allocator
  basic_key_value_pair(basic_key_value_pair &&other, const allocator_type &alloc) noexcept
      : m_key(std::move(other.m_key)),
        m_length(std::move(other.m_length)),
        m_value(std::move(other.m_value), alloc) {
    other.m_key = nullptr;
    other.m_length = 0;
  }

  basic_key_value_pair &operator=(const basic_key_value_pair &) = default;
  basic_key_value_pair &operator=(basic_key_value_pair &&) noexcept = default;

  ~basic_key_value_pair() noexcept {
    priv_deallocate_key();
  }

  /// \brief Returns the stored key.
  /// \return Returns the stored key.
  const key_type key() const noexcept {
    return metall::to_raw_pointer(m_key);
  }

  /// \brief Returns the stored key as const char*.
  /// \return Returns the stored key as const char*.
  const char *key_c_str() const noexcept {
    return metall::to_raw_pointer(m_key);
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

  bool priv_allocate_key(key_type key) {
    char_allocator_type alloc(m_value.get_allocator());
    assert(!m_key);
    m_length = key.length();
    m_key = std::allocator_traits<char_allocator_type>::allocate(alloc, m_length + 1);
    if (!m_key) {
      return false;
    }
    assert(m_key);

    std::char_traits<char_type>::copy(metall::to_raw_pointer(m_key), key.data(), m_length);
    std::char_traits<char_type>::assign(m_key[m_length], '\0');

    return true;
  }

  bool priv_deallocate_key() {
    char_allocator_type alloc(m_value.get_allocator());
    std::allocator_traits<char_allocator_type>::deallocate(alloc, m_key, m_length);
    return true;
  }

  char_pointer m_key{nullptr};
  std::size_t m_length{0};
  value_type m_value;
};

/// \brief A basic_key_value_pair type that uses char as its char type.
template <typename allocator_type = std::allocator<std::byte>>
using key_value_pair = basic_key_value_pair<char, std::char_traits<char>, allocator_type>;

} // namespace json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP
