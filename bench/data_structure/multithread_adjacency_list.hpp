// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP
#define METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP

#include <iostream>
#include <vector>
#include <unordered_map>
#include <cassert>
#include <mutex>
#include <algorithm>

#include <boost/interprocess/containers/vector.hpp>
#include <boost/unordered_map.hpp>
#include <boost/container/scoped_allocator.hpp>

#include "../utility/hash.hpp"

namespace data_structure {

namespace {
namespace bip = boost::interprocess;
namespace bct = boost::container;
}

constexpr std::size_t k_default_num_banks = 256;

template <typename _key_type, typename _value_type, typename _base_allocator_type = std::allocator<void>>
class multithread_adjacency_list {
 public:
  using key_type = _key_type;
  using value_type = _value_type;

 private:
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<_base_allocator_type>::template rebind_alloc<T>;

  using list_allocator_type = other_allocator_type<value_type>;
  using list_type = bip::vector<value_type, list_allocator_type>;

  using key_table_allocator_type = bct::scoped_allocator_adaptor<other_allocator_type<std::pair<const key_type,
                                                                                                list_type>>>;
  using key_table_type = boost::unordered_map<key_type, list_type, utility::hash<key_type>, std::equal_to<key_type>,
                                              key_table_allocator_type>;

  using bank_table_allocator_type = bct::scoped_allocator_adaptor<other_allocator_type<key_table_type>>;
  using bank_table_t = std::vector<key_table_type, bank_table_allocator_type>;

  // Forward declaration
  template <bool is_const>
  class impl_key_iterator;

 public:
  using key_iterator = impl_key_iterator<false>;
  using const_key_iterator = impl_key_iterator<true>;
  using value_iterator = typename list_type::iterator;
  using const_value_iterator = typename list_type::const_iterator;
  using local_key_iterator = typename key_table_type::iterator;
  using const_local_key_iterator = typename key_table_type::const_iterator;

  explicit multithread_adjacency_list(const _base_allocator_type &allocator = _base_allocator_type())
      : m_bank_table(k_default_num_banks, allocator),
        m_mutex(k_default_num_banks) {}

  explicit multithread_adjacency_list(const std::size_t num_banks,
                                      const _base_allocator_type &allocator = _base_allocator_type())
      : m_bank_table(num_banks, allocator),
        m_mutex(num_banks) {}

  ~multithread_adjacency_list() = default;

  bool add(key_type key, value_type value) {
    std::lock_guard<std::mutex> guard(m_mutex[bank_index(key)]);
    key_table_of(key)[key].emplace_back(std::move(value));

    return true;
  }

  std::size_t num_keys() const {
    std::size_t count = 0;
    for (auto &table : m_bank_table) {
      count += table.size();
    }
    return count;
  }

  std::size_t num_values(const key_type &key) const {
    if (key_table_of(key).count(key) == 0) return 0;
    return key_table_of(key).at(key).size();
  }

  // Keys must be const
  const_key_iterator keys_begin() const {
    auto itr = const_key_iterator(*this);
    return itr;
  }

  const_key_iterator keys_end() const {
    auto itr = const_key_iterator(*this);
    itr.move_to_end();
    return itr;
  }

  value_iterator values_begin(const key_type &key) {
    return key_table_of(key).at(key).begin();
  }

  value_iterator values_end(const key_type &key) {
    return key_table_of(key).at(key).end();
  }

  const_value_iterator values_begin(const key_type &key) const {
    return key_table_of(key).at(key).begin();
  }

  const_value_iterator values_end(const key_type &key) const {
    return key_table_of(key).at(key).end();
  }

  const_local_key_iterator keys_begin(const std::size_t bank_index) const {
    assert(bank_index < num_banks());
    return m_bank_table[bank_index].begin();
  }

  const_local_key_iterator keys_end(const std::size_t bank_index) const {
    assert(bank_index < num_banks());
    return m_bank_table[bank_index].end();
  }

  std::size_t num_banks() const {
    return m_bank_table.size();
  }

 private:
  std::size_t bank_index(const std::size_t i) const {
    return i % m_bank_table.size();
  }

  key_table_type &key_table_of(const key_type &src) {
    const auto index = bank_index(src);
    return m_bank_table[index];
  }

  const key_table_type &key_table_of(const key_type &src) const {
    const auto index = bank_index(src);
    return m_bank_table[index];
  }

  bank_table_t m_bank_table;
  std::vector<std::mutex> m_mutex;
};

template <typename _key_type, typename _value_type, typename _base_allocator_type>
template <bool is_const>
class multithread_adjacency_list<_key_type, _value_type, _base_allocator_type>::impl_key_iterator {
 private:
  using adjacency_list_type = typename std::conditional<is_const,
                                                        const multithread_adjacency_list<_key_type,
                                                                                         _value_type,
                                                                                         _base_allocator_type>,
                                                        multithread_adjacency_list<_key_type,
                                                                                   _value_type,
                                                                                   _base_allocator_type>
  >::type;
  using local_iterator_type = typename std::conditional<is_const,
                                                        typename adjacency_list_type::key_table_type::const_iterator,
                                                        typename adjacency_list_type::key_table_type::iterator
  >::type;

 public:
  using difference_type = typename local_iterator_type::difference_type;
  using value_type = typename local_iterator_type::value_type;
  using pointer = typename local_iterator_type::pointer;
  using reference = typename local_iterator_type::reference;

  explicit impl_key_iterator(adjacency_list_type &adjacency_list)
      : m_ref_adjacency_list(adjacency_list),
        m_current_bank_index(0),
        m_local_iterator(m_ref_adjacency_list.m_bank_table[0].begin()) {
    while (m_local_iterator == m_ref_adjacency_list.m_bank_table[m_current_bank_index].end()) {
      ++m_current_bank_index; // move to the next bank
      if (m_current_bank_index == m_ref_adjacency_list.m_bank_table.size()) {
        return;  // reach the end
      }
      m_local_iterator = m_ref_adjacency_list.m_bank_table[m_current_bank_index].begin();
    }
  }

  void move_to_end() {
    m_current_bank_index = m_ref_adjacency_list.m_bank_table.size();
    m_local_iterator = m_ref_adjacency_list.m_bank_table[m_ref_adjacency_list.m_bank_table.size() - 1].end();
  }

  bool operator==(const impl_key_iterator &other) {
    return (m_current_bank_index == other.m_current_bank_index && m_local_iterator == other.m_local_iterator);
  }

  bool operator!=(const impl_key_iterator &other) {
    return !(*this == other);
  }

  reference operator++() {
    next();
    return *m_local_iterator;
  }

  impl_key_iterator operator++(int) {
    impl_key_iterator tmp(*this);
    operator++();
    return tmp;
  }

  pointer operator->() {
    return &(*m_local_iterator);
  }

  reference operator*() {
    return (*m_local_iterator);
  }

 private:

  void next() {
    if (m_current_bank_index == m_ref_adjacency_list.m_bank_table.size()) {
      assert(m_local_iterator == m_ref_adjacency_list.m_bank_table[m_ref_adjacency_list.m_bank_table.size() - 1].end());
      return; // already at the end
    }

    ++m_local_iterator;
    if (m_local_iterator != m_ref_adjacency_list.m_bank_table[m_current_bank_index].end()) return; // found the next one

    // iterate until find the next valid element
    while (true) {
      ++m_current_bank_index; // move to next bank
      if (m_current_bank_index == m_ref_adjacency_list.m_bank_table.size()) {
        assert(
            m_local_iterator == m_ref_adjacency_list.m_bank_table[m_ref_adjacency_list.m_bank_table.size() - 1].end());
        return;  // reach the end
      }
      m_local_iterator = m_ref_adjacency_list.m_bank_table[m_current_bank_index].begin();
      if (m_local_iterator != m_ref_adjacency_list.m_bank_table[m_current_bank_index].end())
        return; // found the next one
    }
  }

  adjacency_list_type &m_ref_adjacency_list;
  std::size_t m_current_bank_index;
  local_iterator_type m_local_iterator;
};

} // namespace data_structure

#endif //METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP
