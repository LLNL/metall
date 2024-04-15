// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cstddef>

#include <metall/metall.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "../data_structure/partitioned_multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace bfs_bench;
using namespace data_structure;

using vertex_id_type = uint64_t;

using local_adjacency_list_type = multithread_adjacency_list<
    vertex_id_type, vertex_id_type,
    typename metall::manager::allocator_type<std::byte>>;
using adjacency_list_type =
    partitioned_multithread_adjacency_list<local_adjacency_list_type>;

int main(int argc, char *argv[]) {
  bench_options<vertex_id_type> option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  {
    std::vector<metall::manager *> managers;
    for (const auto &file_name : option.graph_file_name_list) {
      managers.emplace_back(
          new metall::manager(metall::open_read_only, file_name));
    }

    auto adj_list = adjacency_list_type(option.graph_key_name, managers.begin(),
                                        managers.end());

    run_bench(adj_list, option);
  }

  return 0;
}