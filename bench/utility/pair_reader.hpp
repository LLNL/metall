// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_PAIR_READER_HPP
#define METALL_BENCH_UTILITY_PAIR_READER_HPP

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <queue>
#include <deque>
#include <iterator>
#include <limits>
#include <cassert>

namespace bench_utility {

template <typename first_type, typename second_type>
class pair_reader {
 public:
  /// TODO: add more varieties of constructor
  template <typename input_itr>
  pair_reader<first_type, second_type>(input_itr first, input_itr last)
      : m_file_name_list(std::deque<std::string>(first, last)) {}

  class iterator;

  iterator begin() { return iterator(m_file_name_list); }

  iterator end() { return iterator(); }

 private:
  const std::queue<std::string> m_file_name_list;
};

template <typename first_type, typename second_type>
class pair_reader<first_type, second_type>::iterator {
 public:
  using value_type = std::pair<first_type, second_type>;
  using difference_type = int64_t;
  using category = std::input_iterator_tag;
  using pointer_type = value_type *;
  using referece_type = value_type &;

  iterator() : m_value(), m_current_file_name(), m_file_name_queue(), m_ifs() {
    set_as_end();
  }

  explicit iterator(std::queue<std::string> queue)
      : m_value(),
        m_current_file_name(queue.front()),
        m_file_name_queue(std::move(queue)),
        m_ifs() {
    read_line();
  }

  iterator(const iterator &other)
      : m_value(other.m_value),
        m_current_file_name(other.m_current_file_name),
        m_file_name_queue(other.m_file_name_queue),
        m_ifs() {
    open_file();
  }

  iterator(iterator &&) = default;  // TODO: make sure this works

  /// Copy and move assignments
  iterator &operator=(iterator rhs) {
    rhs.swap(*this);
    return (*this);
  }

  inline void swap(iterator &other) {
    using std::swap;
    swap(m_value, other.m_value);
    swap(m_current_file_name, other.m_current_file_name);
    swap(m_file_name_queue, other.m_file_name_queue);
    swap(m_ifs, other.m_ifs);
  }

  referece_type operator*() { return m_value; }

  pointer_type operator->() { return &m_value; }

  iterator &operator++() {
    read_line();
    return *this;
  }

  iterator operator++(int) {
    iterator tmp(*this);
    operator++();
    return tmp;
  }

  inline bool operator==(iterator &other) const { return equal(other); }

  inline bool operator!=(iterator &other) const {
    return !(this->operator==(other));
  }

  bool equal(const iterator &other) const {
    return (m_value == other.m_value
            // && m_ifs == other.m_ifs
            && m_current_file_name == other.m_current_file_name &&
            m_file_name_queue == other.m_file_name_queue);
  }

 private:
  void read_line() {
    while (true) {
      if (m_ifs >> m_value.first >> m_value.second) {
        break;
      }
      if (!open_next_file()) {
        set_as_end();
        break;
      }
    }
  }

  void open_file() {
    if (m_ifs.is_open()) m_ifs.close();
    m_ifs.open(m_current_file_name);
    if (!m_ifs.is_open()) {
      std::cerr << __FILE__ << ":" << __LINE__ << " Can not open "
                << m_current_file_name << std::endl;
      std::abort();
    }
  }

  bool open_next_file() {
    if (m_file_name_queue.empty()) {
      return false;
    }
    m_current_file_name = m_file_name_queue.front();
    m_file_name_queue.pop();
    open_file();

    return true;
  }

  void set_as_end() {
    m_value = std::move(value_type());

    m_current_file_name.clear();

    assert(m_file_name_queue.empty());
    m_file_name_queue = std::queue<std::string>();

    if (m_ifs.is_open()) m_ifs.close();
  }

  value_type m_value;
  std::string m_current_file_name;
  std::queue<std::string> m_file_name_queue;
  std::ifstream m_ifs;  // not used in equal()
};

// template <typename first_type, typename second_type>
// inline bool operator==(const typename pair_reader<first_type,
// second_type>::iterator &rhd,
//                        const typename pair_reader<first_type,
//                        second_type>::iterator &lhd)
//{
//     return rhd.equal(lhd);
// }
//
// template <typename first_type, typename second_type>
// inline bool operator!=(const typename pair_reader<first_type,
// second_type>::iterator &rhd,
//                        const typename pair_reader<first_type,
//                        second_type>::iterator &lhd)
//{
//   return !(rhd == lhd);
// }

}  // namespace bench_utility
#endif  // METALL_BENCH_UTILITY_PAIR_READER_HPP
