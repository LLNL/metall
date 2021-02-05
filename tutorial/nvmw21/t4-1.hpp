// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TUTORIAL_NVMW21_T4_1_HPP
#define METALL_TUTORIAL_NVMW21_T4_1_HPP

#include <iostream>
#include <memory>
#include <boost/container/vector.hpp>

// An example of an allocator-aware data structure
// This data structure does not contain any code that is only required to Metall
template <typename T, typename Alloc = std::allocator<T>>
class dynamic_array {
 public:
  explicit dynamic_array(Alloc alloc = Alloc()) :
      m_array(alloc) {}

  T &operator[](int pos) {
    return m_array[pos];
  }

  void resize(int n) {
    m_array.resize(n);
  }

 private:
  boost::container::vector<T, Alloc> m_array;
};

template<typename array_t>
void init(array_t& array) {
  array.resize(5);
  for (int i = 0; i < 5; ++i) array[i] = i;
}

template<typename array_t>
void print(array_t& array) {
  for (int i = 0; i < 5; ++i) std::cout << array[i] << std::endl;
}

#endif //METALL_TUTORIAL_NVMW21_T4_1_HPP
