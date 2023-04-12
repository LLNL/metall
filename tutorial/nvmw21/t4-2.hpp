// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TUTORIAL_NVMW21_T4_2_HPP
#define METALL_TUTORIAL_NVMW21_T4_2_HPP

#include <memory>
#include <boost/container/vector.hpp>
#include <boost/container/scoped_allocator.hpp>

namespace {
namespace mc = boost::container;
}

// An example of an allocator-aware data structure (2D array)
// It is to be emphasized that this data structure does not contain any code
// that is only dedicated for Metall
template <typename T, typename Alloc = std::allocator<T>>
class matrix {
 public:
  explicit matrix(Alloc alloc = Alloc()) : m_matrix(alloc) {}

  // Change capacity
  void resize(int num_rows, int num_cols) {
    m_matrix.resize(num_rows);
    for (auto& vec : m_matrix) {
      vec.resize(num_cols);
    }
  }

  // Set a value
  void set(int row, int col, T value) { m_matrix[row][col] = value; }

  // Returns a value
  T get(int row, int col) { return m_matrix[row][col]; }

 private:
  using inner_alloc = typename Alloc::template rebind<T>::other;
  using inner_vector = mc::vector<T, inner_alloc>;

  using outer_alloc = typename Alloc::template rebind<inner_vector>::other;
  using outer_vector =
      mc::vector<inner_vector, mc::scoped_allocator_adaptor<outer_alloc>>;

  outer_vector m_matrix;
};

// --------------------
//  Helper functions
// --------------------
template <typename matrix_t>
void init_matrix(matrix_t& mx) {
  mx.resize(2, 2);
  float value = 0;
  for (int r = 0; r < 2; ++r)
    for (int c = 0; c < 2; ++c) mx.set(r, c, value++);
}

template <typename matrix_t>
void print_matrix(matrix_t& mx) {
  for (int r = 0; r < 2; ++r) {
    for (int c = 0; c < 2; ++c) {
      std::cout << mx.get(r, c) << " ";
    }
    std::cout << std::endl;
  }
}

#endif  // METALL_TUTORIAL_NVMW21_T4_2_HPP
