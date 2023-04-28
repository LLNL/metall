// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_USING_VECTOR_HPP
#define METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_USING_VECTOR_HPP

#include <memory>
#include <boost/container/vector.hpp>

namespace bc = boost::container;

/// \brief Simple CSR graph data structure that can take a custom C++ allocator
/// and be stored in persistent memory This class uses vector containers to
/// allocate internal arrays. \tparam index_t Index type \tparam vid_t Vertex ID
/// type \tparam allocator_t Allocator type
template <typename index_t = uint64_t, typename vid_t = uint64_t,
          typename allocator_t = std::allocator<char>>
class csr_using_vector {
 private:
  // Here, we declare allocator and vector types we need.
  // To store the indices array and edges array in persistent memory,
  // we have to pass the allocator type (allocator_t) to the vectors.

  // Allocator and vector types for the indices array
  using index_allocator_t = typename std::allocator_traits<
      allocator_t>::template rebind_alloc<index_t>;
  using index_vector_t = bc::vector<index_t, index_allocator_t>;

  // Allocator and vector types for the edges array
  using edge_allocator_t =
      typename std::allocator_traits<allocator_t>::template rebind_alloc<vid_t>;
  using edge_vector_t = bc::vector<vid_t, edge_allocator_t>;

 public:
  csr_using_vector(const std::size_t num_vertices, const std::size_t num_edges,
                   allocator_t allocator = allocator_t())
      : m_num_vertices(num_vertices),
        m_num_edges(num_edges),
        m_indices(m_num_vertices + 1, allocator),
        m_edges(m_num_edges, allocator) {}

  /// \brief Returns a raw pointer to the indices array
  index_t *indices() { return m_indices.data(); }

  /// \brief Returns a raw pointer to the edges array
  vid_t *edges() { return m_edges.data(); }

 private:
  const std::size_t m_num_vertices;
  const std::size_t m_num_edges;
  index_vector_t m_indices;
  edge_vector_t m_edges;
};

#endif  // METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_USING_VECTOR_HPP
