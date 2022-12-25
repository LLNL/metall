// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP

#include <string_view>
#include <memory>
#include <string>
#include <algorithm>
#include <cstring>

#include <metall/offset_ptr.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

namespace jsndtl {
/// \brief Provides 'equal' calculation for other key-value types that have the same interface as the object class.
template <typename char_type, typename char_traits, typename allocator_type, typename other_key_value_pair_type>
inline bool general_key_value_pair_equal(const key_value_pair<char_type, char_traits, allocator_type> &key_value,
                                         const other_key_value_pair_type &other_key_value) noexcept {
  if (std::strcmp(key_value.c_str(), other_key_value.c_str()) != 0) return false;
  return key_value.value() == other_key_value.value();
}
} // namespace jsndtl

/// \brief A class for holding a pair of JSON string (as its key) and JSON value (as its value).
/// \tparam char_type A char type to store.
/// \tparam char_traits A chart traits.
/// \tparam _allocator_type An allocator type.
template <typename _char_type = char,
          typename _char_traits = std::char_traits<_char_type>,
          typename _allocator_type = std::allocator<_char_type>>
class key_value_pair {
 private:
  using char_allocator_type = typename std::allocator_traits<_allocator_type>::template rebind_alloc<_char_type>;
  using char_pointer = typename std::allocator_traits<char_allocator_type>::pointer;

 public:
  using char_type = _char_type;
  using char_traits = _char_traits;
  using allocator_type = _allocator_type;
  using key_type = std::basic_string_view<char_type, char_traits>;
  using value_type = metall::container::experimental::json::value<allocator_type>;
  using size_type = std::size_t;

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  key_value_pair(key_type key,
                 const value_type &value,
                 const allocator_type &alloc = allocator_type())
      : m_value(value, alloc) {
    priv_allocate_key(key.data(), key.length());
  }

  /// \brief Constructor.
  /// \param key A key string.
  /// \param value A JSON value to hold.
  /// \param alloc An allocator object to allocate the key and the contents of the JSON value.
  key_value_pair(key_type key,
                 value_type &&value,
                 const allocator_type &alloc = allocator_type())
      : m_value(std::move(value), alloc) {
    priv_allocate_key(key.data(), key.length());
  }

  /// \brief Copy constructor
  key_value_pair(const key_value_pair &other)
      : m_value(other.m_value) {
    if (other.m_key_length <= k_short_key_max_length) {
      priv_allocate_key(other.m_short_key, other.m_key_length);
    } else {
      priv_allocate_key(metall::to_raw_pointer(other.m_long_key), other.m_key_length);
    }
  }

  /// \brief Allocator-extended copy constructor
  key_value_pair(const key_value_pair &other, const allocator_type &alloc)
      : m_value(other.m_value, alloc) {
    if (other.m_key_length <= k_short_key_max_length) {
      priv_allocate_key(other.m_short_key, other.m_key_length);
    } else {
      priv_allocate_key(metall::to_raw_pointer(other.m_long_key), other.m_key_length);
    }
  }

  /// \brief Move constructor
  key_value_pair(key_value_pair &&other) noexcept
      : m_key_data(std::move(other.m_key_data)),
        m_key_length(other.m_key_length),
        m_value(std::move(other.m_value)) {
    other.m_key_length = 0;
  }

  /// \brief Allocator-extended move constructor
  key_value_pair(key_value_pair &&other, const allocator_type &alloc) noexcept
      : m_value(std::move(other.m_value), alloc) {
    if (other.m_key_length <= k_short_key_max_length) {
      priv_allocate_key(other.m_short_key, other.m_key_length);
    } else {
      priv_allocate_key(metall::to_raw_pointer(other.m_long_key), other.m_key_length);
    }
    other.m_key_length = 0;
  }

  /// \brief Copy assignment operator
  key_value_pair &operator=(const key_value_pair &other) {
    priv_deallocate_key();  // deallocate the key using the current allocator first

    m_value = other.m_value; // Assign value (new allocator is assigned)

    if (other.m_key_length <= k_short_key_max_length) {
      priv_allocate_key(other.m_short_key, other.m_key_length);
    } else {
      priv_allocate_key(metall::to_raw_pointer(other.m_long_key), other.m_key_length);
    }

    return *this;
  }

  /// \brief Move assignment operator
  key_value_pair &operator=(key_value_pair &&other) noexcept {
    priv_deallocate_key(); // deallocate the key using the current allocator first

    if (get_allocator() == other.get_allocator()) {
      m_value = std::move(other.m_value);
      m_key_data = std::move(other.m_key_data);
      m_key_length = other.m_key_length;
      other.m_key_length = 0;
    } else {
      priv_allocate_key(metall::to_raw_pointer(other.m_long_key), other.m_key_length);
      other.priv_deallocate_key();
      m_value = std::move(other.m_value);
    }

    return *this;
  }

  /// \brief Swap contents.
  void swap(key_value_pair &other) noexcept {
    using std::swap;
    if constexpr (!std::is_same_v<typename std::allocator_traits<allocator_type>::propagate_on_container_swap,
                                  std::true_type>) {
      // This is an undefined behavior in the C++ standard.
      assert(get_allocator() == other.m_allocator);
    }
    swap(m_key_data, other.m_key_data);
    swap(m_key_length, other.m_key_length);
    swap(m_value, other.m_value);
  }

  /// \brief Destructor
  ~key_value_pair() noexcept {
    priv_deallocate_key();
  }

  /// \brief Returns the stored key.
  /// \return Returns the stored key.
  const key_type key() const noexcept {
    return key_type(key_c_str(), m_key_length);
  }

  /// \brief Returns the stored key as const char*.
  /// \return Returns the stored key as const char*.
  const char_type *key_c_str() const noexcept {
    if (m_key_length <= k_short_key_max_length) {
      return m_short_key;
    }
    return metall::to_raw_pointer(m_long_key);
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

  /// \brief Return `true` if two key-value pairs are equal.
  /// \param lhs A key-value pair to compare.
  /// \param rhs A key-value pair to compare.
  /// \return True if two key-value pairs are equal. Otherwise, false.
  friend bool operator==(const key_value_pair &lhs, const key_value_pair &rhs) noexcept {
    return jsndtl::general_key_value_pair_equal(lhs, rhs);
  }

  /// \brief Return `true` if two key-value pairs are not equal.
  /// \param lhs A key-value pair to compare.
  /// \param rhs A key-value pair to compare.
  /// \return True if two key-value pairs are not equal. Otherwise, false.
  friend bool operator!=(const key_value_pair &lhs, const key_value_pair &rhs) noexcept {
    return !(lhs == rhs);
  }

  /// \brief Return an allocator object.
  allocator_type get_allocator() const noexcept {
    return m_value.get_allocator();
  }

 private:
  static constexpr uint32_t k_short_key_max_length = sizeof(char_pointer) - 1; // -1 for '0'

  bool priv_allocate_key(const char_type *const key, const size_type length) {
    assert(m_key_length == 0);
    m_key_length = length;

    if (m_key_length <= k_short_key_max_length) {
      std::char_traits<char_type>::copy(m_short_key, key, m_key_length);
      std::char_traits<char_type>::assign(m_short_key[m_key_length], '\0');
    } else {
      char_allocator_type alloc(m_value.get_allocator());
      m_long_key = std::allocator_traits<char_allocator_type>::allocate(alloc, m_key_length + 1);
      if (!m_long_key) {
        m_key_length = 0;
        std::abort(); // TODO: change
        return false;
      }

      std::char_traits<char_type>::copy(metall::to_raw_pointer(m_long_key), key, m_key_length);
      std::char_traits<char_type>::assign(m_long_key[m_key_length], '\0');
    }

    return true;
  }

  bool priv_deallocate_key() {
    if (m_key_length > k_short_key_max_length) {
      char_allocator_type alloc(m_value.get_allocator());
      std::allocator_traits<char_allocator_type>::deallocate(alloc, m_long_key, m_key_length + 1);
    }

    m_key_length = 0;

    return true;
  }

  union {
    char_pointer m_long_key{nullptr};
    char m_short_key[k_short_key_max_length + 1]; // + 1 for '\0'

    static_assert(sizeof(char_pointer) == sizeof(uint64_t),
                  "sizeof(char_pointer) is not equal to sizeof(uint64_t)");
    uint64_t m_key_data;
  };
  size_type m_key_length{0};
  value_type m_value;
};

/// \brief Swap value instances.
template <typename allocator_type>
inline void swap(key_value_pair<allocator_type> &lhd, key_value_pair<allocator_type> &rhd) noexcept {
  lhd.swap(rhd);
}

} // namespace json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_KEY_VALUE_PAIR_HPP