// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_HPP
#define METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_HPP

#include <memory>
#include <metall/metall.hpp>

/// \brief Simple CSR graph data structure that can take a custom C++ allocator
/// and be stored in persistent memory \tparam index_t Index type \tparam vid_t
/// Vertex ID type \tparam allocator_t Allocator type
template <typename index_t = uint64_t, typename vid_t = uint64_t,
          typename allocator_t = std::allocator<char>>
class csr {
 private:
  // Here, we declare allocator and pointer types we need.
  // As this data structure can be stored in persistent memory,
  // we have to use the pointer type the given allocator has

  // Allocator and pointer types for the indices array
  using index_allocator_t = typename std::allocator_traits<
      allocator_t>::template rebind_alloc<index_t>;
  using index_pointer_t =
      typename std::allocator_traits<index_allocator_t>::pointer;

  // Allocator and pointer types for the edges array
  using edge_allocator_t =
      typename std::allocator_traits<allocator_t>::template rebind_alloc<vid_t>;
  using edge_pointer_t =
      typename std::allocator_traits<edge_allocator_t>::pointer;

 public:
  csr(const std::size_t num_vertices, const std::size_t num_edges,
      allocator_t allocator = allocator_t())
      : m_num_vertices(num_vertices),
        m_num_edges(num_edges),
        m_indices(nullptr),
        m_edges(nullptr),
        m_allocator(allocator) {
    // -- Allocate arrays with dedicated allocators -- //
    auto index_allocator = index_allocator_t(m_allocator);
    m_indices = index_allocator.allocate(m_num_vertices + 1);
    auto edge_allocator = edge_allocator_t(m_allocator);
    m_edges = edge_allocator.allocate(num_edges);
  }

  ~csr() {
    // -- Deallocate arrays with dedicated allocators -- //
    auto index_allocator = index_allocator_t(m_allocator);
    index_allocator.deallocate(m_indices, m_num_vertices + 1);
    auto edge_allocator = edge_allocator_t(m_allocator);
    edge_allocator.deallocate(m_edges, m_num_edges);
  }

  /// \brief Returns a raw pointer to the indices array
  index_t *indices() {
    return metall::to_raw_pointer(m_indices);
    // return std::pointer_traits<char_pointer_t>::to_address(m_indices); //
    // From C++20
  }

  /// \brief Returns a raw pointer to the edges array
  vid_t *edges() {
    return metall::to_raw_pointer(m_edges);
    // return std::pointer_traits<char_pointer_t>::to_address(m_edges); // From
    // C++20
  }

 private:
  const std::size_t m_num_vertices;
  const std::size_t m_num_edges;
  index_pointer_t m_indices;
  edge_pointer_t m_edges;
  allocator_t m_allocator;
};

#endif  // METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_CSR_HPP
