// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_COMPACT_OBJECT_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_COMPACT_OBJECT_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <string_view>

#include <boost/container/vector.hpp>
#include <boost/container/deque.hpp>

#include <metall/utility/hash.hpp>
#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

namespace {
namespace bc = boost::container;
}

/// \brief JSON object.
/// An object is an unordered map of key and value pairs.
template <typename _allocator_type = std::allocator<std::byte>>
class object {
 public:
  using allocator_type = _allocator_type;
  using value_type = key_value_pair<allocator_type>;
  using key_type = std::string_view; //typename value_type::key_type;
  using mapped_type = value<allocator_type>; //typename value_type::value_type;

 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<typename std::allocator_traits<alloc>::template rebind_alloc<
      T>>;

  using value_storage_alloc_type = other_scoped_allocator<allocator_type, value_type>;
  using value_storage_type = bc::vector<value_type, value_storage_alloc_type>;

  // Value: the position of the corresponding item in the value_storage
  using value_postion_type = typename value_storage_type::size_type;

 public:

  using iterator = typename value_storage_type::iterator;
  using const_iterator = typename value_storage_type::const_iterator;

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit object(const allocator_type &alloc = allocator_type())
      : m_value_storage(alloc) {}

  /// \brief Access a mapped value with a key.
  /// If there is no mapped value that is associated with 'key', allocates it first.
  /// \param index The key of the mapped value to access.
  /// \return A reference to the mapped value associated with 'key'.
  mapped_type &operator[](std::string_view key) {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage[pos].value();
    }

    const auto emplaced_pos = priv_emplace_value(key, mapped_type{m_value_storage.get_allocator()});
    return m_value_storage[emplaced_pos].value();
  }

  /// \brief Access a mapped value.
  /// \param index The key of the mapped value to access.
  /// \return A reference to the mapped value associated with 'key'.
  const mapped_type &operator[](std::string_view key) const {
    return m_value_storage[priv_locate_value(key)].value();
  }

  iterator find(std::string_view key) {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage.begin() + pos;
    }
    return m_value_storage.end();
  }

  const_iterator find(std::string_view key) const {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage.cbegin() + pos;
    }
    return m_value_storage.cend();
  }

  /// \brief Returns an iterator that is at the beginning of the objects.
  /// \return An iterator that is at the beginning of the objects.
  iterator begin() {
    return m_value_storage.begin();
  }

  /// \brief Returns an iterator that is at the beginning of the objects.
  /// \return A const iterator that is at the beginning of the objects.
  const_iterator begin() const {
    return m_value_storage.begin();
  }

  /// \brief Returns an iterator that is at the end of the objects.
  /// \return An iterator that is at the end of the objects.
  iterator end() {
    return m_value_storage.end();
  }

  /// \brief Returns an iterator that is at the end of the objects.
  /// \return A const iterator that is at the end of the objects.
  const_iterator end() const {
    return m_value_storage.end();
  }

  /// \brief Returns the number of key-value pairs.
  /// \return The number of key-values pairs.
  std::size_t size() const {
    return m_value_storage.size();
  }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is returned.
  iterator erase(iterator position) {
    return priv_erase(position);
  }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is returned.
  iterator erase(const_iterator position) {
    return priv_erase(position);
  }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is returned.
  iterator erase(std::string_view key) {
    return priv_erase(find(key));
  }

 private:

  value_postion_type priv_locate_value(std::string_view key) const {
    for (value_postion_type i = 0; i < m_value_storage.size(); ++i) {
      if (m_value_storage[i].key() == key) {
        return i; // Found the key
      }
    }
    return m_value_storage.max_size(); // Couldn't find
  }

  value_postion_type priv_emplace_value(std::string_view key, mapped_type mapped_value) {
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

  value_storage_type m_value_storage;
};

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_COMPACT_OBJECT_HPP
