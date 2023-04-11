// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_DETAILS_COMPACT_OBJECT_HPP
#define METALL_JSON_DETAILS_COMPACT_OBJECT_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <string_view>
#include <algorithm>

#include <metall/json/json_fwd.hpp>
#include <metall/container/scoped_allocator.hpp>
#include <metall/container/vector.hpp>
#include <metall/utility/hash.hpp>

namespace metall::json::jsndtl {

namespace {
namespace mc = metall::container;
}

// Forward declarations
template <typename Alloc = std::allocator<std::byte>>
class compact_object;

template <typename allocator_type, typename other_object_type>
bool general_compact_object_equal(
    const compact_object<allocator_type> &object,
    const other_object_type &other_object) noexcept;

/// \brief JSON object implementation.
/// This class is designed to use a small amount of memory even sacrificing the
/// look-up performance.
template <typename Alloc>
class compact_object {
 public:
  using allocator_type = Alloc;
  using value_type =
      key_value_pair<char, std::char_traits<char>, allocator_type>;
  using key_type = std::basic_string_view<
      char, std::char_traits<char>>;          // typename value_type::key_type;
  using mapped_type = value<allocator_type>;  // typename
                                              // value_type::value_type;

 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = mc::scoped_allocator_adaptor<
      typename std::allocator_traits<alloc>::template rebind_alloc<T>>;

  using value_storage_alloc_type =
      other_scoped_allocator<allocator_type, value_type>;
  using value_storage_type = mc::vector<value_type, value_storage_alloc_type>;

  // Value: the position of the corresponding item in the value_storage
  using value_postion_type = typename value_storage_type::size_type;

 public:
  using iterator = typename value_storage_type::iterator;
  using const_iterator = typename value_storage_type::const_iterator;

  /// \brief Constructor.
  compact_object() {}

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit compact_object(const allocator_type &alloc)
      : m_value_storage(alloc) {}

  /// \brief Copy constructor
  compact_object(const compact_object &) = default;

  /// \brief Allocator-extended copy constructor
  compact_object(const compact_object &other, const allocator_type &alloc)
      : m_value_storage(other.m_value_storage, alloc) {}

  /// \brief Move constructor
  compact_object(compact_object &&) noexcept = default;

  /// \brief Allocator-extended move constructor
  compact_object(compact_object &&other, const allocator_type &alloc) noexcept
      : m_value_storage(std::move(other.m_value_storage), alloc) {}

  /// \brief Copy assignment operator
  compact_object &operator=(const compact_object &) = default;

  /// \brief Move assignment operator
  compact_object &operator=(compact_object &&) noexcept = default;

  /// \brief Swap contents.
  void swap(compact_object &other) noexcept {
    using std::swap;
    swap(m_value_storage, other.m_value_storage);
  }

  /// \brief Access a mapped value with a key.
  /// If there is no mapped value that is associated with 'key', allocates it
  /// first. \param key The key of the mapped value to access. \return A
  /// reference to the mapped value associated with 'key'.
  mapped_type &operator[](const key_type &key) {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage[pos].value();
    }

    const auto emplaced_pos =
        priv_emplace_value(key, mapped_type{m_value_storage.get_allocator()});
    return m_value_storage[emplaced_pos].value();
  }

  /// \brief Access a mapped value.
  /// \param key The key of the mapped value to access.
  /// \return A reference to the mapped value associated with 'key'.
  const mapped_type &operator[](const key_type &key) const {
    return m_value_storage[priv_locate_value(key)].value();
  }

  /// \brief Return true if the key is found.
  /// \return True if found; otherwise, false.
  bool contains(const key_type &key) const { return count(key) > 0; }

  /// \brief Count the number of elements with a specific key.
  /// \return The number elements with a specific key.
  std::size_t count(const key_type &key) const {
    const auto pos = priv_locate_value(key);
    return pos < m_value_storage.max_size() ? 1 : 0;
  }

  /// \brief Access a mapped value.
  /// \param key The key of the mapped value to access.
  /// \return A reference to the mapped value associated with 'key'.
  mapped_type &at(const key_type &key) {
    return m_value_storage[priv_locate_value(key)].value();
  }

  /// \brief Access a mapped value.
  /// \param key The key of the mapped value to access.
  /// \return A reference to the mapped value associated with 'key'.
  const mapped_type &at(const key_type &key) const {
    return m_value_storage[priv_locate_value(key)].value();
  }

  iterator find(const key_type &key) {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage.begin() + pos;
    }
    return m_value_storage.end();
  }

  const_iterator find(const key_type &key) const {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage.cbegin() + pos;
    }
    return m_value_storage.cend();
  }

  /// \brief Returns an iterator that is at the beginning of the objects.
  /// \return An iterator that is at the beginning of the objects.
  iterator begin() { return m_value_storage.begin(); }

  /// \brief Returns an iterator that is at the beginning of the objects.
  /// \return A const iterator that is at the beginning of the objects.
  const_iterator begin() const { return m_value_storage.begin(); }

  /// \brief Returns an iterator that is at the end of the objects.
  /// \return An iterator that is at the end of the objects.
  iterator end() { return m_value_storage.end(); }

  /// \brief Returns an iterator that is at the end of the objects.
  /// \return A const iterator that is at the end of the objects.
  const_iterator end() const { return m_value_storage.end(); }

  /// \brief Returns the number of key-value pairs.
  /// \return The number of key-values pairs.
  std::size_t size() const { return m_value_storage.size(); }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is
  /// returned.
  iterator erase(iterator position) { return priv_erase(position); }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is
  /// returned.
  iterator erase(const_iterator position) { return priv_erase(position); }

  /// \brief Erases the element associated with 'key'.
  /// \param key The key of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is
  /// returned.
  iterator erase(const key_type &key) { return priv_erase(find(key)); }

  /// \brief Return `true` if two objects are equal.
  /// \param lhs An object to compare.
  /// \param rhs An object to compare.
  /// \return True if two objects are equal. Otherwise, false.
  friend bool operator==(const compact_object &lhs,
                         const compact_object &rhs) noexcept {
    return jsndtl::general_compact_object_equal(lhs, rhs);
  }

  /// \brief Return `true` if two objects are not equal.
  /// \param lhs An object to compare.
  /// \param rhs An object to compare.
  /// \return True if two objects are not equal. Otherwise, false.
  friend bool operator!=(const compact_object &lhs,
                         const compact_object &rhs) noexcept {
    return !(lhs == rhs);
  }

  /// \brief Return an allocator object.
  allocator_type get_allocator() const noexcept {
    return m_value_storage.get_allovator();
  }

 private:
  value_postion_type priv_locate_value(const key_type &key) const {
    for (value_postion_type i = 0; i < m_value_storage.size(); ++i) {
      if (m_value_storage[i].key() == key) {
        return i;  // Found the key
      }
    }
    return m_value_storage.max_size();  // Couldn't find
  }

  value_postion_type priv_emplace_value(const key_type &key,
                                        mapped_type &&mapped_value) {
    m_value_storage.emplace_back(key, std::move(mapped_value));
    return m_value_storage.size() - 1;
  }

  auto priv_erase(const_iterator value_position) {
    if (value_position == m_value_storage.cend()) {
      return m_value_storage.end();
    }

    // Finally, erase the value.
    return m_value_storage.erase(value_position);
  }

  value_storage_type m_value_storage{allocator_type{}};
};

/// \brief Swap value instances.
template <typename allocator_type>
inline void swap(compact_object<allocator_type> &lhd,
                 compact_object<allocator_type> &rhd) noexcept {
  lhd.swap(rhd);
}

/// \brief Provides 'equal' calculation for other object types that have the
/// same interface as the object class.
template <typename allocator_type, typename other_object_type>
inline bool general_compact_object_equal(
    const compact_object<allocator_type> &object,
    const other_object_type &other_object) noexcept {
  if (object.size() != other_object.size()) return false;

  for (const auto &key_value : object) {
    auto itr = other_object.find(key_value.key_c_str());
    if (itr == other_object.end()) return false;
    if (key_value.value() != itr->value()) return false;
  }

  return true;
}

}  // namespace metall::json::jsndtl

#endif  // METALL_JSON_DETAILS_COMPACT_OBJECT_HPP
