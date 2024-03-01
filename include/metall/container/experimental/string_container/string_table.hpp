// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_CONTAINERS_STRING_TABLE_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_CONTAINERS_STRING_TABLE_HPP

#include <memory>
#include <scoped_allocator>
#include <string_view>
#include <utility>

#include <metall/metall.hpp>
#include <metall/container/unordered_map.hpp>
#include <metall/container/string.hpp>

namespace metall::container::experimental::string_container {

template <typename Alloc = metall::manager::allocator_type<std::byte>>
class string_table {
 public:
  using allocator_type = Alloc;
  using key_type = std::string_view;
  using locator_type = uint64_t;

 private:
  template <typename T>
  using other_allocator =
      typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator =
      std::scoped_allocator_adaptor<other_allocator<T>>;

  using id_type = uint64_t;

  using string_type =
      metall::container::basic_string<char, std::char_traits<char>,
                                      other_allocator<char>>;
  using map_allocator_type =
      other_scoped_allocator<std::pair<const id_type, string_type>>;
  using map_type =
      metall::container::unordered_map<id_type, string_type, std::hash<id_type>,
                                       std::equal_to<id_type>,
                                       map_allocator_type>;

  /// Used for representing 'invalid key'.
  static constexpr id_type k_max_internal_id =
      std::numeric_limits<id_type>::max();

 public:
  static constexpr id_type invalid_locator = k_max_internal_id;

  string_table() = default;

  explicit string_table(const allocator_type &allocator) : m_map(allocator) {}

  explicit string_table(const uint64_t hash_seed,
                        const allocator_type &allocator = allocator_type())
      : m_hash_seed(hash_seed), m_map(allocator) {}

  // Delete all for now.
  // When implement them, make sure to copy compact_string explicitly.
  string_table(const string_table &) = delete;
  string_table(string_table &&) = delete;
  string_table &operator=(const string_table &) = delete;
  string_table &operator=(string_table &&) = delete;

  ~string_table() noexcept {
    try {
      clear();
    } catch (...) {
    }
  }

  /// \brief Inserts a new element with 'key' if it does not exist.
  /// If the element with 'key' already exists, returns the locator that
  /// corresponds to 'key'.
  /// \param key The key of the element to insert.
  /// \return A locator that corresponds to 'key'.
  locator_type insert(const key_type &key) {
    const auto locator = priv_find_internal_id(key);
    if (locator != k_max_internal_id) {
      return locator;
    }
    const auto id = priv_generate_internal_id(key);
    m_map[id] = string_type(key.data(), key.length(), get_allocator());
    return id;
  }

  bool contains(const key_type &key) const {
    return to_locator(key) != invalid_locator;
  }

  locator_type to_locator(const key_type &key) const {
    const auto locator = priv_find_internal_id(key);
    return locator;
  }

  key_type to_key(const locator_type &locator) const {
    assert(m_map.count(locator) == 1);
    return m_map.at(locator).c_str();
  }

  void clear() {
    m_max_id_probe_distance = 0;
    m_map.clear();
  }

  std::size_t size() const { return m_map.size(); }

  allocator_type get_allocator() const { return m_map.get_allocator(); }

 private:
  /// \brief Finds the internal ID that corresponds to 'key'.
  /// If this container does not have an element with 'key',
  /// Generate a new internal ID.
  id_type priv_find_or_generate_internal_id(const key_type &key) {
    auto internal_id = priv_find_internal_id(key);
    if (internal_id == k_max_internal_id) {  // Couldn't find
      // Generate a new one.
      internal_id = priv_generate_internal_id(key);
    }
    return internal_id;
  }

  /// \brief Generates a new internal ID for 'key'.
  id_type priv_generate_internal_id(const key_type &key) {
    auto internal_id = priv_hash_key(key, m_hash_seed);

    std::size_t distance = 0;
    while (m_map.count(internal_id) > 0) {
      internal_id = priv_increment_internal_id(internal_id);
      ++distance;
    }
    m_max_id_probe_distance = std::max(distance, m_max_id_probe_distance);

    return internal_id;
  }

  /// \brief Finds the internal ID that corresponds with 'key'.
  /// If this container does not have an element with 'key',
  /// returns k_max_internal_id.
  id_type priv_find_internal_id(const key_type &key) const {
    auto internal_id = priv_hash_key(key, m_hash_seed);

    for (std::size_t d = 0; d <= m_max_id_probe_distance; ++d) {
      const auto itr = m_map.find(internal_id);
      if (itr == m_map.end()) {
        break;
      }

      if (itr->second == key) {
        return internal_id;
      }
      internal_id = priv_increment_internal_id(internal_id);
    }

    return k_max_internal_id;  // Couldn't find
  }

  static id_type priv_hash_key(const key_type &key,
                               [[maybe_unused]] const uint64_t seed) {
    auto hash = (id_type)metall::mtlldetail::MurmurHash64A(
        key.data(), (int)key.length(), seed);
    if (hash == k_max_internal_id) {
      hash = priv_increment_internal_id(hash);
    }
    assert(hash != k_max_internal_id);
    return hash;
  }

  static id_type priv_increment_internal_id(const id_type id) {
    const auto new_id = (id + 1) % k_max_internal_id;
    assert(new_id != k_max_internal_id);
    return new_id;
  }

  uint64_t m_hash_seed{123};
  std::size_t m_max_id_probe_distance{0};
  map_type m_map;
};

}  // namespace metall::container::experimental::string_container

#endif  // METALL_CONTAINER_EXPERIMENT_STRING_CONTAINERS_STRING_TABLE_HPP
