// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace bfs_bench;

using vertex_id_type = uint64_t;

namespace bip = boost::interprocess;
using allocator_type =
    bip::allocator<void, bip::managed_mapped_file::segment_manager>;
using adjacency_list_type =
    data_structure::multithread_adjacency_list<vertex_id_type, vertex_id_type,
                                               allocator_type>;

int main(int argc, char *argv[]) {
  bench_options<vertex_id_type> option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  {
    bip::managed_mapped_file mfile(bip::open_only,
                                   option.graph_file_name_list[0].c_str());
    auto adj_list =
        mfile.find<adjacency_list_type>(option.graph_key_name.c_str()).first;

    run_bench(*adj_list, option);
  }

  return 0;
}