// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>

#include <metall/metall.hpp>
#include "graph_data_structure/csr.hpp"
#include "graph_data_structure/csr_using_vector.hpp"

using index_t = uint64_t;
using vid_t = uint64_t;

// We have two CSR graph data structures that have the same interfaces
#if 0
using csr_graph_t = csr<index_t, vid_t, metall::manager::allocator_type<char>>;
#else
using csr_graph_t =
    csr_using_vector<index_t, vid_t, metall::manager::allocator_type<char>>;
#endif

int main() {
  {
    // Create a new Metall datastore in "/tmp/dir"
    // If 'dir' does not exist, Metall creates automatically
    metall::manager manager(metall::create_only, "/tmp/dir");

    std::size_t num_vertices = 16;
    std::size_t num_edges = 256;

    // Allocate and construct an object in the persistent memory with a name
    // "csr_graph"
    csr_graph_t *csr_graph = manager.construct<csr_graph_t>("csr_graph")(
        num_vertices, num_edges,
        manager.get_allocator());  // Arguments to the constructor

    // You can use the arrays normally
    index_t *indices_array = csr_graph->indices();
    vid_t *edges_array = csr_graph->edges();
    edges_array[indices_array[1]++] = 10;
  }

  {
    // Open the existing Metall datastore in "/tmp/dir"
    metall::manager manager(metall::open_read_only, "/tmp/dir");

    csr_graph_t *csr_graph = manager.find<csr_graph_t>("csr_graph").first;
    if (!csr_graph) {
      std::cerr << "Object csr_graph does not exist" << std::endl;
      std::abort();
    }

    // You can use the arrays normally
    index_t *indices_array = csr_graph->indices();
    vid_t *edges_array = csr_graph->edges();
    std::cout << edges_array[indices_array[0]] << std::endl;
  }

  return 0;
}