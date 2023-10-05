// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP
#define METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP

#include <iostream>
#include <cassert>
#include <limits>
#include <utility>

#include <boost/graph/rmat_graph_generator.hpp>
#include <metall/utility/random.hpp>
#include <metall/utility/hash.hpp>

namespace edge_generator {

/// \brief Iterator for rmat_edge_generator class
template <typename parent_type>
class rmat_edge_generator_iterator {
 private:
  using rnd_generator_type = metall::utility::rand_512;

 public:
  using value_type = std::pair<uint64_t, uint64_t>;
  using const_reference = const value_type &;
  using const_pointer = const value_type *;

  explicit rmat_edge_generator_iterator(const parent_type &parent,
                                        const std::size_t offset)
      : m_ptr_parent(&parent),
        m_rnd_generator(nullptr),
        m_current_edge(),
        m_generate_reverse_edge(true),
        m_num_generated_edges(offset) {
    if (m_num_generated_edges < m_ptr_parent->m_num_edges) {
      m_rnd_generator.reset(new boost::uniform_01<rnd_generator_type>(
          rnd_generator_type(m_ptr_parent->m_seed)));
      generate_new_edge();
    }
  }

  rmat_edge_generator_iterator(const rmat_edge_generator_iterator &) = default;
  rmat_edge_generator_iterator(rmat_edge_generator_iterator &&) = default;
  rmat_edge_generator_iterator &operator=(
      const rmat_edge_generator_iterator &) = default;
  rmat_edge_generator_iterator &operator=(rmat_edge_generator_iterator &&) =
      default;

  const_reference operator*() const { return m_current_edge; }

  const_pointer operator->() const { return &m_current_edge; }

  rmat_edge_generator_iterator &operator++() {
    next();
    return *(this);
  }

  const rmat_edge_generator_iterator operator++(int) {
    rmat_edge_generator_iterator tmp(*this);
    operator++();
    return tmp;
  }

  bool operator==(const rmat_edge_generator_iterator &other) const {
    return m_num_generated_edges == other.m_num_generated_edges;
  }

  bool operator!=(const rmat_edge_generator_iterator &other) const {
    return !(*this == other);
  }

 private:
  void next() {
    if (m_ptr_parent->m_undirected & m_generate_reverse_edge) {
      std::swap(m_current_edge.first, m_current_edge.second);
      m_generate_reverse_edge = false;
      return;
    }

    generate_new_edge();

    if (m_ptr_parent->m_undirected) m_generate_reverse_edge = true;
  }

  void generate_new_edge() {
    m_current_edge =
        generate_edge(m_rnd_generator, 1ULL << m_ptr_parent->m_vertex_scale,
                      m_ptr_parent->m_vertex_scale, m_ptr_parent->m_a,
                      m_ptr_parent->m_b, m_ptr_parent->m_c, m_ptr_parent->m_d);
    if (m_ptr_parent->m_scramble_id) {
      const uint64_t mask = (1ULL << m_ptr_parent->m_vertex_scale) - 1;
      // Assume utility::hash is a good hash function
      m_current_edge.first =
          metall::utility::hash<>()(m_current_edge.first) & mask;
      m_current_edge.second =
          metall::utility::hash<>()(m_current_edge.second) & mask;
    }
    ++m_num_generated_edges;
  }

  const parent_type *const m_ptr_parent;
  boost::shared_ptr<boost::uniform_01<rnd_generator_type>> m_rnd_generator;
  value_type m_current_edge;
  bool m_generate_reverse_edge;
  std::size_t m_num_generated_edges;
};

/// \brief R-MAT Edge Generator
class rmat_edge_generator {
 public:
  using iterator = rmat_edge_generator_iterator<rmat_edge_generator>;
  friend iterator;

  rmat_edge_generator(const uint32_t seed, const uint64_t vertex_scale,
                      const uint64_t num_edges, const double a, const double b,
                      const double c, const bool scramble_id,
                      const bool undirected)
      : m_seed(seed),
        m_vertex_scale(vertex_scale),
        m_num_edges(num_edges),
        m_a(a),
        m_b(b),
        m_c(c),
        m_d(1.0 - (m_a + m_b + m_c)),
        m_scramble_id(scramble_id),
        m_undirected(undirected) {
    if ((m_a <= m_b || m_a <= m_c || m_a <= m_d) ||
        (m_a + m_b + m_c + m_d != 1.0)) {
      std::cerr << "Unexpected parameter(s)" << std::endl;
      std::abort();
    }
  }

  iterator begin() const { return iterator(*this, 0); }

  iterator end() const { return iterator(*this, m_num_edges + 1); }

 private:
  const uint32_t m_seed;
  const uint64_t m_vertex_scale;
  const uint64_t m_num_edges;
  const double m_a;
  const double m_b;
  const double m_c;
  const double m_d;
  const bool m_scramble_id;
  const bool m_undirected;
};

}  // namespace edge_generator

#endif  // METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP
