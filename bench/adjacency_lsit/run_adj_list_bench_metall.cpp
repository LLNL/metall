// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <vector>

#include <metall/manager.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"
#include "../utility/time.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type, value_type, typename metall::manager::allocator_type<void>>;

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.segment_file_name.empty()) {
    std::cerr << "Segment file name is required" << std::endl;
    std::abort();
  }

  {
    metall::manager manager(metall::create_only, option.segment_file_name.c_str(), option.segment_size);
    auto adj_list = manager.construct<adjacency_list_type>(option.adj_list_key_name.c_str())(manager.get_allocator<>());

    run_bench(option, adj_list);

    const auto start = utility::elapsed_time_sec();
    manager.sync();
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "sync_time (s)\t" << elapsed_time << std::endl;

    manager.profile("/tmp/metall_profile.log");
  }

  return 0;
}