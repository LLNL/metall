// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP
#define METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP

#include <iostream>
#include <cassert>
#include <limits>

#include <boost/random/mersenne_twister.hpp>
#include <boost/graph/rmat_graph_generator.hpp>
#include <boost/graph/adjacency_list.hpp>

namespace edge_generator {

/// \brief Iterator for rmat_edge_generator class
class rmat_edge_generator_iterator {
 private:
  using rnd_generator_type = boost::random::mt19937;
  using boost_graph_type = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS>;
  using rmat_iterator_type = boost::rmat_iterator<rnd_generator_type, boost_graph_type>;

 public:
  using value_type = rmat_iterator_type::value_type;
  using reference = rmat_iterator_type::reference;
  using pointer = rmat_iterator_type::pointer;

  rmat_edge_generator_iterator()
      : m_rnd_generator(),
        m_rmat_iterator(),
        m_undirected(),
        m_current_edge(),
        m_generate_reverse_edge() {}

  // !!! CAUTION !!!
  // In Boost 1.69, boost::rmat_iterator generates one less than the number of specified edges and
  // cannot generate more than '2^31 - 1' edges as it uses a 'int' variable to count edges.
  rmat_edge_generator_iterator(const uint32_t seed, const uint64_t vertex_scale, const uint64_t num_edges,
                               const double a, const double b, const double c, const double d,
                               const bool scramble_id, const bool undirected)
      : m_rnd_generator(seed),
        m_rmat_iterator(m_rnd_generator, 1ULL << vertex_scale, num_edges + 1, a, b, c, d, scramble_id),
        m_undirected(undirected),
        m_current_edge(*m_rmat_iterator),
        m_generate_reverse_edge(true) {
    if (num_edges > std::numeric_limits<int>::max()) {
      std::cerr << "Too many edges to generate: " << num_edges << std::endl;
      std::abort();
    }
  }

  rmat_edge_generator_iterator(const rmat_edge_generator_iterator &) = default;
  rmat_edge_generator_iterator(rmat_edge_generator_iterator &&) = default;
  rmat_edge_generator_iterator &operator=(const rmat_edge_generator_iterator &) = default;
  rmat_edge_generator_iterator &operator=(rmat_edge_generator_iterator &&) = default;

  reference operator*() const {
    return m_current_edge;
  }

  pointer operator->() const {
    return &m_current_edge;
  }

  rmat_edge_generator_iterator &operator++() {
    next();
    return *(this);
  }

  rmat_edge_generator_iterator operator++(int) {
    rmat_edge_generator_iterator tmp(*this);
    operator++();
    return tmp;
  }

  bool operator==(const rmat_edge_generator_iterator &other) const {
    return m_rmat_iterator == other.m_rmat_iterator;
  }

  bool operator!=(const rmat_edge_generator_iterator &other) const {
    return !(*this == other);
  }

 private:
  void next() {
    if (m_undirected & m_generate_reverse_edge) {
      std::swap(m_current_edge.first, m_current_edge.second);
      m_generate_reverse_edge = false;
      return;
    }

    ++m_rmat_iterator; // You can do '*m_rmat_iterator' even after m_rmat_iterator is at the end
    m_current_edge = *m_rmat_iterator;

    if (m_undirected)
      m_generate_reverse_edge = true;
  }

  rnd_generator_type m_rnd_generator;
  rmat_iterator_type m_rmat_iterator;
  const bool m_undirected;
  value_type m_current_edge;
  bool m_generate_reverse_edge;
  std::size_t m_num_generated_edges;
};

/// \brief R-MAT Edge Generator
class rmat_edge_generator {
 public:
  using iterator = rmat_edge_generator_iterator;

  rmat_edge_generator(const uint32_t seed, const uint64_t vertex_scale, const uint64_t num_edges,
                      const double a, const double b, const double c,
                      const bool scramble_id, const bool undirected)
      : m_seed(seed),
        m_vertex_scale(vertex_scale),
        m_num_edges(num_edges),
        m_a(a),
        m_b(b),
        m_c(c),
        m_d(1.0 - (m_a + m_b + m_c)),
        m_scramble_id(scramble_id),
        m_undirected(undirected) {
    if ((m_a <= m_b || m_a <= m_c || m_a <= m_d) || (m_a + m_b + m_c + m_d != 1.0)) {
      std::cerr << "Unexpected parameter(s)" << std::endl;
      std::abort();
    }
  }

  iterator begin() const {
    return iterator(m_seed, m_vertex_scale, m_num_edges, m_a, m_b, m_c, m_d, m_scramble_id, m_undirected);
  }

  iterator end() const {
    return iterator();
  }

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

}

#endif //METALL_BENCH_ADJACENCY_LIST_RMAT_EDGE_GENERATOR_HPP
