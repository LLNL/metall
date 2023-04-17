// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_CONCURRENT__STRING_KEY_STORE_LOCATOR_HPP_
#define METALL_CONTAINER_CONCURRENT__STRING_KEY_STORE_LOCATOR_HPP_

namespace metall::container {

template <typename iterator_type>
class string_key_store_locator {
 private:
  template <typename value_type, typename allocator_type>
  friend class string_key_store;

 public:
  string_key_store_locator(iterator_type iterator) : m_iterator(iterator) {}

  string_key_store_locator &operator++() {
    ++m_iterator;
    return *this;
  }

  string_key_store_locator operator++(int) {
    auto tmp(*this);
    ++m_iterator;
    return tmp;
  }

  bool operator==(const string_key_store_locator &other) const {
    return m_iterator == other.m_iterator;
  }

  bool operator!=(const string_key_store_locator &other) const {
    return m_iterator != other.m_iterator;
  }

 private:
  iterator_type m_iterator;
};

}  // namespace metall::container

#endif  // METALL_CONTAINER_CONCURRENT__STRING_KEY_STORE_LOCATOR_HPP_
