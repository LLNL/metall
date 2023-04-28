// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_DATA_STRUCTURE_PARTITIONED_MULTITHREAD_ADJACENCY_LIST_HPP
#define METALL_BENCH_DATA_STRUCTURE_PARTITIONED_MULTITHREAD_ADJACENCY_LIST_HPP

#include <vector>
#include <memory>
#include <cstddef>

namespace data_structure {

template <typename local_adj_list_type,
          typename global_allocator_type = std::allocator<std::byte>>
class partitioned_multithread_adjacency_list {
 public:
  using key_type = typename local_adj_list_type::key_type;
  using value_type = typename local_adj_list_type::value_type;

 private:
  template <typename T>
  using actual_global_allocator_type = typename std::allocator_traits<
      global_allocator_type>::template rebind_alloc<T>;

  using global_adjacency_list_type =
      std::vector<local_adj_list_type *,
                  actual_global_allocator_type<local_adj_list_type *>>;

  // Forward declaration
  class impl_key_iterator;

 public:
  using const_key_iterator = impl_key_iterator;
  using value_iterator = typename local_adj_list_type::value_iterator;
  using const_value_iterator =
      typename local_adj_list_type::const_value_iterator;
  ;
  using const_local_key_iterator =
      typename local_adj_list_type::const_key_iterator;
  ;

  template <typename local_manager_iterator>
  partitioned_multithread_adjacency_list(
      const std::string &key_name, local_manager_iterator first,
      local_manager_iterator last,
      const global_allocator_type &global_allocator = global_allocator_type())
      : m_global_adjacency_list(global_allocator) {
    for (; first != last; ++first) {
      auto manager = *first;
      auto adj_list = manager->template find_or_construct<local_adj_list_type>(
          key_name.c_str())(manager->template get_allocator<>());
      m_global_adjacency_list.push_back(adj_list);
    }
  }

  bool add(key_type key, value_type value) {
    local_adjacency_list_of(key).add(std::move(key), std::move(value));
    return true;
  }

  std::size_t num_keys() const {
    std::size_t count = 0;
    for (auto &local : m_global_adjacency_list) {
      count += local.num_keys();
    }
    return count;
  }

  std::size_t num_values(const key_type &key) const {
    if (local_adjacency_list_of(key).num_values(key) == 0) return 0;
    return local_adjacency_list_of(key).num_values(key);
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
    return local_adjacency_list_of(key).values_begin(key);
  }

  value_iterator values_end(const key_type &key) {
    return local_adjacency_list_of(key).values_end(key);
  }

  const_value_iterator values_begin(const key_type &key) const {
    return local_adjacency_list_of(key).values_begin(key);
  }

  const_value_iterator values_end(const key_type &key) const {
    return local_adjacency_list_of(key).values_end(key);
  }

  const_local_key_iterator keys_begin(const std::size_t partition_index) const {
    assert(partition_index < num_partition());
    return m_global_adjacency_list[partition_index].keys_begin();
  }

  const_local_key_iterator keys_end(const std::size_t partition_index) const {
    assert(partition_index < num_partition());
    return m_global_adjacency_list[partition_index].keys_end();
  }

  std::size_t num_partition() const { return m_global_adjacency_list.size(); }

  std::size_t partition_index(const key_type &key) const {
    return key % num_partition();
  }

  void sync() const {
    for (auto adj_list : m_global_adjacency_list) {
      adj_list->sync();
    }
  }

 private:
  local_adj_list_type &local_adjacency_list_of(const std::size_t i) {
    return *m_global_adjacency_list[partition_index(i)];
  }

  const local_adj_list_type &local_adjacency_list_of(
      const std::size_t i) const {
    return *m_global_adjacency_list[partition_index(i)];
  }

  global_adjacency_list_type m_global_adjacency_list;
};

template <typename local_adj_list_type, typename global_allocator_type>
class partitioned_multithread_adjacency_list<
    local_adj_list_type, global_allocator_type>::impl_key_iterator {
 private:
  using global_adjacency_list_type =
      partitioned_multithread_adjacency_list<local_adj_list_type,
                                             global_allocator_type>;

 public:
  using difference_type = typename const_local_key_iterator::difference_type;
  using value_type = typename const_local_key_iterator::value_type;
  using pointer = typename const_local_key_iterator::pointer;
  using reference = typename const_local_key_iterator::reference;

  explicit impl_key_iterator(const global_adjacency_list_type *adjacency_list)
      : m_ptr_adjacency_list(adjacency_list),
        m_current_partition(0),
        m_local_iterator(
            m_ptr_adjacency_list->m_global_adjacency_list[0]->keys_begin()) {
    while (m_local_iterator ==
           m_ptr_adjacency_list->m_global_adjacency_list[m_current_partition]
               ->keys_end()) {
      ++m_current_partition;  // move to the next bank
      if (m_current_partition == m_ptr_adjacency_list->num_partition()) {
        return;  // reach the end
      }
      m_local_iterator =
          m_ptr_adjacency_list->m_global_adjacency_list[m_current_partition]
              ->keys_begin();
    }
  }

  impl_key_iterator(const impl_key_iterator &) = default;
  impl_key_iterator(impl_key_iterator &&) = default;

  impl_key_iterator &operator=(const impl_key_iterator &) = default;
  impl_key_iterator &operator=(impl_key_iterator &&) = default;

  void move_to_end() {
    m_current_partition = m_ptr_adjacency_list->num_partition();
    m_local_iterator =
        m_ptr_adjacency_list
            ->m_global_adjacency_list[m_ptr_adjacency_list->num_partition() - 1]
            ->keys_end();
  }

  bool operator==(const impl_key_iterator &other) {
    return (m_current_partition == other.m_current_partition &&
            m_local_iterator == other.m_local_iterator);
  }

  bool operator!=(const impl_key_iterator &other) { return !(*this == other); }

  impl_key_iterator &operator++() {
    next();
    return *this;
  }

  impl_key_iterator operator++(int) {
    impl_key_iterator tmp(*this);
    operator++();
    return tmp;
  }

  pointer operator->() { return &(*m_local_iterator); }

  reference operator*() { return (*m_local_iterator); }

 private:
  void next() {
    if (m_current_partition == m_ptr_adjacency_list->num_partition()) {
      assert(
          m_local_iterator ==
          m_ptr_adjacency_list
              ->m_global_adjacency_list[m_ptr_adjacency_list->num_partition() -
                                        1]
              ->keys_end());
      return;  // already at the end
    }

    ++m_local_iterator;
    if (m_local_iterator !=
        m_ptr_adjacency_list->m_global_adjacency_list[m_current_partition]
            ->keys_end())
      return;  // found the next one

    // iterate until find the next valid element
    while (true) {
      ++m_current_partition;  // move to next bank
      if (m_current_partition == m_ptr_adjacency_list->num_partition()) {
        assert(m_local_iterator ==
               m_ptr_adjacency_list
                   ->m_global_adjacency_list
                       [m_ptr_adjacency_list->num_partition() - 1]
                   ->keys_end());
        return;  // reach the end
      }
      m_local_iterator =
          m_ptr_adjacency_list->m_global_adjacency_list[m_current_partition]
              ->keys_begin();
      if (m_local_iterator !=
          m_ptr_adjacency_list->m_global_adjacency_list[m_current_partition]
              ->keys_end())
        return;  // found the next one
    }
  }

  const global_adjacency_list_type *m_ptr_adjacency_list;
  std::size_t m_current_partition;
  const_local_key_iterator m_local_iterator;
};
}  // namespace data_structure

#endif  // METALL_BENCH_DATA_STRUCTURE_PARTITIONED_MULTITHREAD_ADJACENCY_LIST_HPP
