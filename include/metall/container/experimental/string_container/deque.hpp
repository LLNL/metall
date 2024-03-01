// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_DEQUE_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_DEQUE_HPP

#include <memory>

#include <metall/container/deque.hpp>

#include <metall/container/experimental/string_container/string_table.hpp>
#include <metall/container/experimental/string_container/string.hpp>

namespace metall::container::experimental::string_container {

/// \brief A deque container that stores string using the string_table.
template <typename StringTable = string_table<>>
class deque {
 public:
  using allocator_type = typename StringTable::allocator_type;

 private:
  using string_table_type = StringTable;
  using string_type = string<string_table_type>;
  using deque_type = metall::container::deque<
      string_type, std::scoped_allocator_adaptor<typename std::allocator_traits<
                       allocator_type>::template rebind_alloc<string_type>>>;

 public:
  explicit deque(string_table_type *string_table)
      : m_string_table(string_table), m_deque(string_table->get_allocator()) {}

  string_type &operator[](const std::size_t i) { return m_deque[i]; }

  template <typename Str>
  void push_back(const Str &str) {
    m_deque.emplace_back(metall::to_raw_pointer(m_string_table));
    m_deque.back() = str;
  }

  void pop_back() { m_deque.pop_back(); }

  void resize(const std::size_t n) {
    m_deque.resize(n, string_type(metall::to_raw_pointer(m_string_table)));
  }

  void clear() { m_deque.clear(); }

  std::size_t size() const { return m_deque.size(); }

  bool empty() const { return m_deque.empty(); }

  void reserve(const std::size_t n) { m_deque.reserve(n); }

  auto begin() { return m_deque.begin(); }
  auto end() { return m_deque.end(); }
  auto begin() const { return m_deque.begin(); }
  auto end() const { return m_deque.end(); }

  allocator_type get_allocator() const { return m_deque.get_allocator(); }

 private:
  using string_table_ptr_type =
      typename std::pointer_traits<typename std::allocator_traits<
          allocator_type>::pointer>::template rebind<string_table_type>;

  string_table_ptr_type m_string_table;
  deque_type m_deque;
};
}  // namespace metall::container::experimental::string_container
#endif