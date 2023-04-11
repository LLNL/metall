// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_DATA_STRUCTURE_ADJACENCY_LIST_HPP
#define METALL_BENCH_DATA_STRUCTURE_ADJACENCY_LIST_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <memory>
#include <cstddef>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>
#include <boost/container/scoped_allocator.hpp>

namespace data_structure {

namespace {
namespace bip = boost::interprocess;
namespace bct = boost::container;
}  // namespace

template <typename _key_type, typename _value_type,
          typename _base_allocator_type = std::allocator<std::byte>>
class adjacency_list {
 public:
  using key_type = _key_type;
  using value_type = _value_type;

 private:
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<
      _base_allocator_type>::template rebind_alloc<T>;

  using list_allocator_type = other_allocator_type<value_type>;
  using list_type = bip::vector<value_type, list_allocator_type>;

  /// see:
  /// https://stackoverflow.com/questions/36831496/unordered-map-with-string-in-managed-shared-memory-fails
  using key_table_allocator_type = bct::scoped_allocator_adaptor<
      other_allocator_type<std::pair<const key_type, list_type>>>;
  using key_table_type =
      boost::unordered_map<key_type, list_type, std::hash<key_type>,
                           std::equal_to<key_type>, key_table_allocator_type>;

 public:
  using key_iterator = typename key_table_type::iterator;
  using const_key_iterator = typename key_table_type::const_iterator;
  using value_iterator = typename list_type::iterator;
  using const_value_iterator = typename list_type::const_iterator;

  explicit adjacency_list(
      const _base_allocator_type &allocator = _base_allocator_type())
      : m_key_table(allocator) {}

  ~adjacency_list() = default;

  bool add(key_type key, value_type value) {
    m_key_table[key].emplace_back(value);
    return true;
  }

  std::size_t num_keys() const { return m_key_table.size(); }

  std::size_t num_values(const key_type &key) const {
    if (m_key_table.count(key) == 0) return 0;
    return m_key_table.at(key).size();
  }

  key_iterator keys_begin() { return m_key_table.begin(); }

  key_iterator keys_end() { return m_key_table.end(); }

  const_key_iterator keys_begin() const { return m_key_table.begin(); }

  const_key_iterator keys_end() const { return m_key_table.end(); }

  value_iterator values_begin(const key_type &key) {
    return m_key_table[key].begin();
  }

  value_iterator values_end(const key_type &key) {
    return m_key_table[key].end();
  }

  const_value_iterator values_begin(const key_type &key) const {
    return m_key_table.at(key).begin();
  }

  const_value_iterator values_end(const key_type &key) const {
    return m_key_table.at(key).end();
  }

 private:
  key_table_type m_key_table;
};
}  // namespace data_structure

#endif  // METALL_BENCH_DATA_STRUCTURE_ADJACENCY_LIST_HPP
