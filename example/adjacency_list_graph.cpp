// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>

#include <metall/metall.hpp>
#include "graph_data_structure/adjacency_list.hpp"

using vid_t = uint64_t;
using adj_list_graph_t =
    adjacency_list<vid_t, metall::manager::allocator_type<char>>;

int main() {
  {
    // Create a new Metall datastore in "/tmp/dir"
    metall::manager manager(metall::create_only, "/tmp/dir");

    // Allocate and construct an object in the persistent memory with a name
    // "adj_list_graph"
    adj_list_graph_t *adj_list_graph =
        manager.construct<adj_list_graph_t>("adj_list_graph")(
            manager.get_allocator());  // Arguments to the constructor

    // Add an edge (1, 2)
    adj_list_graph->add_edge(1, 2);
  }

  {
    // Open the existing Metall datastore in "/tmp/dir"
    metall::manager manager(metall::open_only, "/tmp/dir");

    adj_list_graph_t *adj_list_graph =
        manager.find<adj_list_graph_t>("adj_list_graph").first;

    // Add another edge
    adj_list_graph->add_edge(1, 3);

    // Print the edges of vertex 1
    for (auto edge = adj_list_graph->edges_begin(1);
         edge != adj_list_graph->edges_end(1); ++edge) {
      std::cout << "1 " << *edge << std::endl;
    }
  }

  return 0;
}