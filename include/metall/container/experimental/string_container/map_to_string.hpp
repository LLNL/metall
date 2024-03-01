// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_MAP_TO_STRING_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_MAP_TO_STRING_HPP

#include <memory>

#include <metall/container/scoped_allocator.hpp>
#include <metall/container/map.hpp>

#include <metall/container/experimental/string_container/string_table.hpp>
#include <metall/container/experimental/string_container/string.hpp>

namespace metall::container::experimental::string_container {

/// \brief A map container that uses string as value and Metall as its default
/// allocator. Internally, it uses string_container::string_table to store the
/// strings. The value is immutable.
/// \tparam Key Type of the key.
/// \tparam Allocator Type of the allocator.
template <class Key, typename StringTable = string_table<>>
class map_to_string {
 public:
  using key_type = Key;
  using mapped_type = string<StringTable>;
  using allocator_type = typename StringTable::allocator_type;

 private:
  using string_table_type = StringTable;
  using string_type = string<string_table_type>;

 public:
  using map_type = metall::container::map<
      key_type, string_type, std::less<key_type>,
      std::scoped_allocator_adaptor<
          typename std::allocator_traits<allocator_type>::template rebind_alloc<
              std::pair<const key_type, string_type>>>>;

  explicit map_to_string(string_table_type *const string_table)
      : m_string_table(string_table), m_map(string_table->get_allocator()) {}

  map_to_string(const map_to_string &) = default;
  map_to_string(map_to_string &&) noexcept = default;
  map_to_string &operator=(const map_to_string &) = default;
  map_to_string &operator=(map_to_string &&) noexcept = default;

  ~map_to_string() noexcept = default;

  template <typename K>
  mapped_type &operator[](const K &key) {
    if (m_map.count(key) == 0) {
      m_map.emplace(key_type(key),
                    mapped_type(metall::to_raw_pointer(m_string_table)));
    }
    return m_map.at(key_type(key));
  }

  template <typename K>
  const mapped_type &at(const K &key) const {
    return m_map.at(key_type(key));
  }

  template <typename K>
  mapped_type &at(const K &key) {
    return m_map.at(key_type(key));
  }

  template <typename K>
  bool contains(const K &key) const {
    return m_map.count(key) > 0;
  }

  void clear() { m_map.clear(); }

  std::size_t size() const { return m_map.size(); }

  bool empty() const { return m_map.empty(); }

  void reserve(const std::size_t n) { m_map.reserve(n); }

  auto begin() { return m_map.begin(); }
  auto end() { return m_map.end(); }
  auto begin() const { return m_map.begin(); }
  auto end() const { return m_map.end(); }

  allocator_type get_allocator() const { return m_map.get_allocator(); }

 private:
  using string_table_ptr_type =
      typename std::pointer_traits<typename std::allocator_traits<
          allocator_type>::pointer>::template rebind<string_table_type>;

  string_table_ptr_type m_string_table;
  map_type m_map;
};

}  // namespace metall::container::experimental::string_container
#endif