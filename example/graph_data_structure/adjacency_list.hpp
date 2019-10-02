// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_ADJACENCY_LIST_HPP
#define METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_ADJACENCY_LIST_HPP

#include <memory>
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/vector.hpp>
#include <boost/unordered_map.hpp>
#include <metall/metall.hpp>

using boost::container::vector;
using boost::unordered_map;
using boost::container::scoped_allocator_adaptor;

/// \brief Simple adjacency-list graph data structure that can take a custom C++ allocator and be stored in persistent memory
/// \tparam vid_t Vertex ID type
/// \tparam allocator_t Allocator type
template <typename vid_t = uint64_t, typename allocator_t = std::allocator<char>>
class adjacency_list {
 private:
  // Inner vector type
  using vector_type = vector<vid_t, metall::manager::allocator_type<vid_t>>;

  // To use custom allocator in multi-level containers, you have to use scoped_allocator_adaptor in the most outer container
  // so that the inner containers obtain their allocator arguments from the outer containers's scoped_allocator_adaptor
  // See: https://en.cppreference.com/w/cpp/memory/scoped_allocator_adaptor
  using map_allocator_type = scoped_allocator_adaptor<metall::manager::allocator_type<std::pair<vid_t, vector_type>>>;
  using map_type = unordered_map<vid_t,
                                 vector_type,
                                 std::hash<vid_t>,
                                 std::equal_to<vid_t>,
                                 map_allocator_type>;

 public:
  explicit adjacency_list(allocator_t allocator = allocator_t())
      : m_map(allocator) {}

  void add_edge(vid_t source, vid_t target) {
    m_map[source].emplace_back(target);
  }

  auto edges_begin(const vid_t source) {
    return m_map[source].begin();
  }

  auto edges_end(const vid_t source) {
    return m_map[source].end();
  }

 private:
  map_type m_map;
};

#endif //METALL_EXAMPLE_GRAPH_DATA_STRUCTURE_ADJACENCY_LIST_HPP
