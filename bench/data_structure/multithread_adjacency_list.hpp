// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP
#define METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP

#include <cassert>
#include <mutex>
#include <algorithm>
#include <cstddef>
#include <functional>

#define METALL_USE_STL_CONTAINERS_IN_ADJLIST 0
#if METALL_USE_STL_CONTAINERS_IN_ADJLIST
#include <vector>
#include <unordered_map>
#include <scoped_allocator>
namespace container = std;
#else
#include <metall/container/vector.hpp>
#include <metall/container/unordered_map.hpp>
#include <metall/container/scoped_allocator.hpp>
namespace container = metall::container;
#endif

#include <metall/utility/hash.hpp>
#include <metall/utility/mutex.hpp>

namespace data_structure {

template <typename _key_type, typename _value_type,
          typename _base_allocator_type = std::allocator<std::byte>>
class multithread_adjacency_list {
 public:
  using key_type = _key_type;
  using value_type = _value_type;
  static constexpr std::size_t k_num_banks = 1024;

 private:
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<
      _base_allocator_type>::template rebind_alloc<T>;

  using list_allocator_type = other_allocator_type<value_type>;
  using list_type = container::vector<value_type, list_allocator_type>;

  using key_table_allocator_type = container::scoped_allocator_adaptor<
      other_allocator_type<std::pair<const key_type, list_type>>>;
  using key_table_type =
      container::unordered_map<key_type, list_type, metall::utility::hash<>,
                               std::equal_to<key_type>,
                               key_table_allocator_type>;

  using bank_table_allocator_type =
      container::scoped_allocator_adaptor<other_allocator_type<key_table_type>>;
  using bank_table_t =
      container::vector<key_table_type, bank_table_allocator_type>;

  // Forward declaration
  class impl_const_key_iterator;

 public:
  using const_key_iterator = impl_const_key_iterator;
  using value_iterator = typename list_type::iterator;
  using const_value_iterator = typename list_type::const_iterator;
  using local_key_iterator = typename key_table_type::iterator;
  using const_local_key_iterator = typename key_table_type::const_iterator;

  explicit multithread_adjacency_list(
      const _base_allocator_type &allocator = _base_allocator_type())
      : m_bank_table(k_num_banks, allocator) {}

  ~multithread_adjacency_list() = default;

  bool add(key_type key, value_type value) {
    auto guard =
        metall::utility::mutex::mutex_lock<k_num_banks>(bank_index(key));
#ifdef __clang__
#if METALL_USE_STL_CONTAINERS_IN_ADJLIST
    m_bank_table[bank_index(key)][key].emplace_back(std::move(value));
#else
    m_bank_table[bank_index(key)][key].emplace_back(std::move(value));
    //    m_bank_table[bank_index(key)].try_emplace(key,
    //    list_allocator_type(m_bank_table.get_allocator()));
    //    m_bank_table[bank_index(key)].at(key).emplace_back(std::move(value));
#endif
#else
    // MEMO: GCC does not work with STL Containers (tested with GCC 10.2.0 on
    // MacOS)
    m_bank_table[bank_index(key)][key].emplace_back(std::move(value));
#endif
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
    if (m_bank_table[bank_index(key)].count(key) == 0) return 0;
    return m_bank_table[bank_index(key)].at(key).size();
  }

  // Keys must be const
  const_key_iterator keys_begin() const {
    auto itr = const_key_iterator(this);
    return itr;
  }

  const_key_iterator keys_end() const {
    auto itr = const_key_iterator(this);
    itr.move_to_end();
    return itr;
  }

  value_iterator values_begin(const key_type &key) {
    return m_bank_table[bank_index(key)].at(key).begin();
  }

  value_iterator values_end(const key_type &key) {
    return m_bank_table[bank_index(key)].at(key).end();
  }

  const_value_iterator values_begin(const key_type &key) const {
    return m_bank_table[bank_index(key)].at(key).begin();
  }

  const_value_iterator values_end(const key_type &key) const {
    return m_bank_table[bank_index(key)].at(key).end();
  }

  const_local_key_iterator keys_begin(const std::size_t bank_index) const {
    assert(bank_index < num_banks());
    return m_bank_table[bank_index].begin();
  }

  const_local_key_iterator keys_end(const std::size_t bank_index) const {
    assert(bank_index < num_banks());
    return m_bank_table[bank_index].end();
  }

  std::size_t num_banks() const { return m_bank_table.size(); }

 private:
  std::size_t bank_index(const std::size_t i) const {
    return i % m_bank_table.size();
  }

  bank_table_t m_bank_table;
};

template <typename _key_type, typename _value_type,
          typename _base_allocator_type>
class multithread_adjacency_list<
    _key_type, _value_type, _base_allocator_type>::impl_const_key_iterator {
 private:
  using adjacency_list_type =
      multithread_adjacency_list<_key_type, _value_type, _base_allocator_type>;
  using local_iterator_type =
      typename adjacency_list_type::key_table_type::const_iterator;

 public:
  using difference_type = typename local_iterator_type::difference_type;
  using value_type = typename local_iterator_type::value_type;
  using pointer = typename local_iterator_type::pointer;
  using reference = typename local_iterator_type::reference;

  explicit impl_const_key_iterator(
      const adjacency_list_type *const adjacency_list)
      : m_ptr_adjacency_list(adjacency_list),
        m_current_bank_index(0),
        m_local_iterator(m_ptr_adjacency_list->m_bank_table[0].begin()) {
    while (m_local_iterator ==
           m_ptr_adjacency_list->m_bank_table[m_current_bank_index].end()) {
      ++m_current_bank_index;  // move to the next bank
      if (m_current_bank_index == m_ptr_adjacency_list->m_bank_table.size()) {
        return;  // reach the end
      }
      m_local_iterator =
          m_ptr_adjacency_list->m_bank_table[m_current_bank_index].begin();
    }
  }

  impl_const_key_iterator(const impl_const_key_iterator &) = default;
  impl_const_key_iterator(impl_const_key_iterator &&) = default;

  impl_const_key_iterator &operator=(const impl_const_key_iterator &) = default;
  impl_const_key_iterator &operator=(impl_const_key_iterator &&) = default;

  void move_to_end() {
    m_current_bank_index = m_ptr_adjacency_list->m_bank_table.size();
    m_local_iterator =
        m_ptr_adjacency_list
            ->m_bank_table[m_ptr_adjacency_list->m_bank_table.size() - 1]
            .end();
  }

  bool operator==(const impl_const_key_iterator &other) {
    return (m_current_bank_index == other.m_current_bank_index &&
            m_local_iterator == other.m_local_iterator);
  }

  bool operator!=(const impl_const_key_iterator &other) {
    return !(*this == other);
  }

  impl_const_key_iterator &operator++() {
    next();
    return *this;
  }

  impl_const_key_iterator operator++(int) {
    impl_const_key_iterator tmp(*this);
    operator++();
    return tmp;
  }

  pointer operator->() { return &(*m_local_iterator); }

  reference operator*() { return (*m_local_iterator); }

 private:
  void next() {
    if (m_current_bank_index == m_ptr_adjacency_list->m_bank_table.size()) {
      assert(m_local_iterator ==
             m_ptr_adjacency_list
                 ->m_bank_table[m_ptr_adjacency_list->m_bank_table.size() - 1]
                 .end());
      return;  // already at the end
    }

    ++m_local_iterator;
    if (m_local_iterator !=
        m_ptr_adjacency_list->m_bank_table[m_current_bank_index].end())
      return;  // found the next one

    // iterate until find the next valid element
    while (true) {
      ++m_current_bank_index;  // move to next bank
      if (m_current_bank_index == m_ptr_adjacency_list->m_bank_table.size()) {
        assert(m_local_iterator ==
               m_ptr_adjacency_list
                   ->m_bank_table[m_ptr_adjacency_list->m_bank_table.size() - 1]
                   .end());
        return;  // reach the end
      }
      m_local_iterator =
          m_ptr_adjacency_list->m_bank_table[m_current_bank_index].begin();
      if (m_local_iterator !=
          m_ptr_adjacency_list->m_bank_table[m_current_bank_index].end())
        return;  // found the next one
    }
  }

  const adjacency_list_type *m_ptr_adjacency_list;
  std::size_t m_current_bank_index;
  local_iterator_type m_local_iterator;
};

}  // namespace data_structure

#endif  // METALL_BENCH_DATA_STRUCTURE_MULTITHREAD_ADJACENCY_LIST_HPP
