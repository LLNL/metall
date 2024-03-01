// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_MAP_FROM_STRING_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_MAP_FROM_STRING_HPP

#include <memory>

#include <metall/container/scoped_allocator.hpp>
#include <metall/container/map.hpp>

#include <metall/container/experimental/string_container/string_table.hpp>
#include <metall/container/experimental/string_container/string.hpp>

namespace metall::container::experimental::string_container {

/// \brief A map container that string as keys. Internally, it uses
/// string_container::string_table to store the keys.
/// \tparam T Type of the value.
/// \tparam StringTable Type of the string table.
template <class T, typename StringTable = string_table<>>
class map_from_string {
 public:
  using key_type = string<StringTable>;
  using mapped_type = T;
  using allocator_type = typename StringTable::allocator_type;

 private:
  using string_table_type = StringTable;
  using key_locator_type = typename string_table_type::locator_type;
  using map_type = metall::container::map<
      key_type, mapped_type, std::less<key_type>,
      std::scoped_allocator_adaptor<
          typename std::allocator_traits<allocator_type>::template rebind_alloc<
              std::pair<const key_type, mapped_type>>>>;

 public:
  explicit map_from_string(string_table_type *const string_table)
      : m_string_table(string_table), m_map(string_table->get_allocator()) {}

  map_from_string(const map_from_string &) = default;
  map_from_string(map_from_string &&) noexcept = default;
  map_from_string &operator=(const map_from_string &) = default;
  map_from_string &operator=(map_from_string &&) noexcept = default;

  ~map_from_string() noexcept = default;

  template <typename K>
  mapped_type &operator[](const K &key) {
    return m_map[get_key(key)];
  }

  template <typename K>
  const mapped_type &at(const K &key) const {
    assert(m_string_table->contains(key) &&
           "The key does not exist in the string table");
    return m_map.at(get_key(key));
  }

  template <typename K>
  bool contains(const K &key) const {
    return m_string_table->contains(key) && m_map.contains(get_key(key));
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

  template <typename K>
  key_type get_key(const K &key) {
    const auto loc = m_string_table->insert(key);
    key_type new_key(metall::to_raw_pointer(m_string_table), loc);
    return new_key;
  }

  template <typename K>
  key_type get_key(const K &key) const {
    const auto loc = m_string_table->to_locator(key);
    key_type new_key(metall::to_raw_pointer(m_string_table), loc);
    return new_key;
  }

  string_table_ptr_type m_string_table;
  map_type m_map;
};

}  // namespace metall::container::experimental::string_container
#endif