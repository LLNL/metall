// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <vector>

#include <metall/metall.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "../data_structure/partitioned_multithread_adjacency_list.hpp"
#include "bench_driver.hpp"
#include "../utility/time.hpp"

using namespace adjacency_list_bench;
using namespace data_structure;

using key_type = uint64_t;
using value_type = uint64_t;

using local_adjacency_list_type =  multithread_adjacency_list<key_type,
                                                              value_type,
                                                              typename metall::manager::allocator_type<void>>;
using adjacency_list_type =  partitioned_multithread_adjacency_list<local_adjacency_list_type>;

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.segment_file_name_list.empty()) {
    std::cerr << "Segment file name is required" << std::endl;
    std::abort();
  }

  {
    std::vector<metall::manager *> managers;
    for (const auto &file_name : option.segment_file_name_list) {
      managers.emplace_back(new metall::manager(metall::create_only, file_name.c_str(), option.segment_size));
    }

    auto adj_list = adjacency_list_type(option.adj_list_key_name, managers.begin(), managers.end());

    run_bench(option, &adj_list);

    const auto start = utility::elapsed_time_sec();
    for (auto manager : managers) {
      manager->sync();
    }
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "sync_time (s)\t" << elapsed_time << std::endl;

    std::cout << "Writing profile" << std::endl;
    for (int i = 0; i < option.segment_file_name_list.size(); ++i) {
      managers[i]->profile("/tmp/metall_profile-" + std::to_string(i) + ".log");
    }
  }

  return 0;
}