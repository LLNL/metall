// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/metall.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace bfs_bench;

using vertex_id_type = uint64_t;

using adjacency_list_type =  data_structure::multithread_adjacency_list<vertex_id_type, vertex_id_type, typename metall::manager::allocator_type<void>>;

int main(int argc, char *argv[]) {

  bench_options<vertex_id_type> option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  {
    metall::manager manager(metall::open_only, option.graph_file_name.c_str());
    auto adj_list = manager.find<adjacency_list_type>(option.graph_key_name.c_str()).first;

    run_bench(*adj_list, option);
  }

  return 0;
}