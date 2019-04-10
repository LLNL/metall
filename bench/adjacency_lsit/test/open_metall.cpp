// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <vector>

#include <metall/metall.hpp>
#include "../../data_structure/adjacency_list.hpp"
#include "../bench_driver.hpp"
#include "../../data_structure/multithread_adjacency_list.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;
using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type, value_type,
                                                                        typename metall::manager::allocator_type<void>>;

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.segment_file_name.empty()) {
    std::cerr << "Out file name is required" << std::endl;
    std::abort();
  }

  {
    metall::manager manager(metall::open_only, option.segment_file_name.c_str());
    auto ret = manager.find<adjacency_list_type>(option.adj_list_key_name.c_str());
    if (ret.second != 1) {
      std::cerr << "Its length is not correct" << std::endl;
      std::abort();
    }

    adjacency_list_type *adj_list = ret.first;
    if (!option.dump_file_name.empty()) {
      dump_adj_list(*adj_list, option.dump_file_name);
    }
  }

  return 0;
}