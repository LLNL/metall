// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_KEY_VALUE_PAIR_HPP
#define METALL_JSON_KEY_VALUE_PAIR_HPP

#include <string_view>
#include <memory>
#include <string>
#include <algorithm>
#include <cstring>

#include <metall/offset_ptr.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/json/json_fwd.hpp>

namespace metall::json {

namespace jsndtl {
/// \brief Provides 'equal' calculation for other key-value types that have the
/// same interface as the object class.
template <typename char_type, typename char_traits, typename allocator_type,
          typename other_key_value_pair_type>
inline bool general_key_value_pair_equal(
    const key_value_pair<char_type, char_traits, allocator_type> &key_value,
    const other_key_value_pair_type &other_key_value) noexcept {
  if (key_value.key().length() != other_key_value.key().length())
    return false;
  if (std::strcmp(key_value.key_c_str(), other_key_value.key_c_str()) != 0)
    return false;
  return key_value.value() == other_key_value.value();
}
}  // namespace jsndtl

/// \brief A class for holding a pair of JSON string (as its key) and JSON value
/// (as its value). \tparam char_type A char type to store. \tparam char_traits
/// A chart traits. \tparam _allocator_type An allocator type.
#ifdef DOXYGEN_SKIP
template <typename char_type = char,
          typename char_traits = std::char_traits<char_type>,
          typename Alloc = std::allocator<char_type>>
#else
template <typename _char_type, typename _char_traits, typename Alloc>
#endif
class key_value_pair {
 private:
  using char_allocator_type =
      typename std::allocator_traits<Alloc>::template rebind_alloc<_char_type>;
  using char_pointer =
      typename std::allocator_traits<char_allocator_type>::pointer;

 public:
  using char_type = _char_type;
  using char_traits = _char_traits;
  using allocator_type = Alloc;
  using key_type = std::basic_string_view<char_type, char_traits>;
  using value_type = metall::json::value<allocator_type>;
  using size_type = std::size_t;

 public:
  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of
  /// the JSON value.
  key_value_pair(key_type key, const value_type &value,
                 const allocator_type &alloc = allocator_type())
      : m_value(value, alloc) {
    priv_allocate_key(key.data(), key.length(), m_value.get_allocator());
  }

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of
  /// the JSON value.
  key_value_pair(key_type key, value_type &&value,
                 const allocator_type &alloc = allocator_type())
      : m_value(std::move(value), alloc) {
    priv_allocate_key(key.data(), key.length(), m_value.get_allocator());
  }

  /// \brief Copy constructor
  key_value_pair(const key_value_pair &other) : m_value(other.m_value) {
    priv_allocate_key(other.key_c_str(), other.m_key_length,
                      m_value.get_allocator());
  }

  /// \brief Allocator-extended copy constructor
  key_value_pair(const key_value_pair &other, const allocator_type &alloc)
      : m_value(other.m_value, alloc) {
    priv_allocate_key(other.key_c_str(), other.m_key_length,
                      m_value.get_allocator());
  }

  /// \brief Move constructor
  key_value_pair(key_value_pair &&other) noexcept
      : m_value(std::move(other.m_value)) {
    if (other.priv_short_key()) {
      m_short_key_buf = other.m_short_key_buf;
    } else {
      m_long_key = std::move(other.m_long_key);
      other.m_long_key = nullptr;
    }
    m_key_length = other.m_key_length;
    other.m_key_length = 0;
  }

  /// \brief Allocator-extended move constructor
  key_value_pair(key_value_pair &&other, const allocator_type &alloc) noexcept
      : m_value(alloc) {
    if (alloc == other.get_allocator()) {
      if (other.priv_short_key()) {
        m_short_key_buf = other.m_short_key_buf;
      } else {
        m_long_key = std::move(other.m_long_key);
        other.m_long_key = nullptr;
      }
      m_key_length = other.m_key_length;
      other.m_key_length = 0;
    } else {
      priv_allocate_key(other.key_c_str(), other.m_key_length, get_allocator());
      other.priv_deallocate_key(other.get_allocator());
    }
    m_value = std::move(other.m_value);
  }

  /// \brief Copy assignment operator
  key_value_pair &operator=(const key_value_pair &other) {
    if (this == &other) return *this;

    priv_deallocate_key(get_allocator());  // deallocate the key using the
                                           // current allocator first
    m_value = other.m_value;
    // This line has to come after copying m_value because m_value decides if
    // allocator is needed to be propagated.
    priv_allocate_key(other.key_c_str(), other.m_key_length, get_allocator());

    return *this;
  }

  /// \brief Move assignment operator
  key_value_pair &operator=(key_value_pair &&other) noexcept {
    if (this == &other) return *this;

    priv_deallocate_key(get_allocator());  // deallocate the key using the
                                           // current allocator first

    auto other_allocator = other.m_value.get_allocator();
    m_value = std::move(other.m_value);

    if (get_allocator() == other.get_allocator()) {
      if (other.priv_short_key()) {
        m_short_key_buf = other.m_short_key_buf;
      } else {
        m_long_key = std::move(other.m_long_key);
        other.m_long_key = nullptr;
      }
      m_key_length = other.m_key_length;
      other.m_key_length = 0;
    } else {
      priv_allocate_key(other.key_c_str(), other.m_key_length, get_allocator());
      other.priv_deallocate_key(other_allocator);
    }

    return *this;
  }

  /// \brief Swap contents.
  void swap(key_value_pair &other) noexcept {
    if constexpr (!std::is_same_v<
                      typename std::allocator_traits<
                          allocator_type>::propagate_on_container_swap,
                      std::true_type>) {
      // This is an undefined behavior in the C++ standard.
      assert(get_allocator() == other.get_allocator());
    }

    using std::swap;

    if (priv_short_key()) {
      swap(m_short_key, other.m_short_key);
    } else {
      swap(m_long_key, other.m_long_key);
    }
    swap(m_key_length, other.m_key_length);

    swap(m_value, other.m_value);
  }

  /// \brief Destructor
  ~key_value_pair() noexcept { priv_deallocate_key(get_allocator()); }

  /// \brief Returns the stored key.
  /// \return Returns the stored key.
  const key_type key() const noexcept {
    return key_type(key_c_str(), m_key_length);
  }

  /// \brief Returns the stored key as const char*.
  /// \return Returns the stored key as const char*.
  const char_type *key_c_str() const noexcept { return priv_key_c_str(); }

  /// \brief References the stored JSON value.
  /// \return Returns a reference to the stored JSON value.
  value_type &value() noexcept { return m_value; }

  /// \brief References the stored JSON value.
  /// \return Returns a reference to the stored JSON value.
  const value_type &value() const noexcept { return m_value; }

  /// \brief Return `true` if two key-value pairs are equal.
  /// \param lhs A key-value pair to compare.
  /// \param rhs A key-value pair to compare.
  /// \return True if two key-value pairs are equal. Otherwise, false.
  friend bool operator==(const key_value_pair &lhs,
                         const key_value_pair &rhs) noexcept {
    return jsndtl::general_key_value_pair_equal(lhs, rhs);
  }

  /// \brief Return `true` if two key-value pairs are not equal.
  /// \param lhs A key-value pair to compare.
  /// \param rhs A key-value pair to compare.
  /// \return True if two key-value pairs are not equal. Otherwise, false.
  friend bool operator!=(const key_value_pair &lhs,
                         const key_value_pair &rhs) noexcept {
    return !(lhs == rhs);
  }

  /// \brief Return an allocator object.
  allocator_type get_allocator() const noexcept {
    return m_value.get_allocator();
  }

 private:
  static constexpr uint32_t k_short_key_max_length =
      sizeof(char_pointer) - 1;  // -1 for '0'

  const char_type *priv_key_c_str() const noexcept {
    if (priv_short_key()) {
      return m_short_key;
    }
    return metall::to_raw_pointer(m_long_key);
  }

  bool priv_short_key() const noexcept {
    return (m_key_length <= k_short_key_max_length);
  }

  bool priv_long_key() const noexcept { return !priv_short_key(); }

  bool priv_allocate_key(const char_type *const key, const size_type length,
                         char_allocator_type alloc) {
    assert(m_key_length == 0);
    m_key_length = length;

    if (priv_short_key()) {
      std::char_traits<char_type>::copy(m_short_key, key, m_key_length);
      std::char_traits<char_type>::assign(m_short_key[m_key_length], '\0');
    } else {
      m_long_key = std::allocator_traits<char_allocator_type>::allocate(
          alloc, m_key_length + 1);
      if (!m_long_key) {
        m_key_length = 0;
        std::abort();  // TODO: change
        return false;
      }

      std::char_traits<char_type>::copy(metall::to_raw_pointer(m_long_key), key,
                                        m_key_length);
      std::char_traits<char_type>::assign(m_long_key[m_key_length], '\0');
    }

    return true;
  }

  bool priv_deallocate_key(char_allocator_type alloc) {
    if (m_key_length > k_short_key_max_length) {
      std::allocator_traits<char_allocator_type>::deallocate(alloc, m_long_key,
                                                             m_key_length + 1);
      m_long_key = nullptr;
    }
    m_key_length = 0;
    return true;
  }

  union {
    char_type m_short_key[k_short_key_max_length + 1];  // + 1 for '\0'
    static_assert(sizeof(char_type) * (k_short_key_max_length + 1) <=
                      sizeof(uint64_t),
                  "sizeof(m_short_key) is bigger than sizeof(uint64_t)");
    uint64_t m_short_key_buf;

    char_pointer m_long_key{nullptr};
  };
  size_type m_key_length{0};

  value_type m_value;
};

/// \brief Swap value instances.
template <typename char_type, typename char_traits, typename allocator_type>
inline void swap(
    key_value_pair<char_type, char_traits, allocator_type> &rhd,
    key_value_pair<char_type, char_traits, allocator_type> &lhd) noexcept {
  lhd.swap(rhd);
}

}  // namespace metall::json

#endif  // METALL_JSON_KEY_VALUE_PAIR_HPP