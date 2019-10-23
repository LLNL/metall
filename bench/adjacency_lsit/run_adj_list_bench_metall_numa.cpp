// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <vector>

#include <metall/metall.hpp>
#include <metall/detail/utility/time.hpp>

#include "../data_structure/multithread_adjacency_list.hpp"
#include "../data_structure/partitioned_multithread_adjacency_list.hpp"
#include "../utility/numa_allocator.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;
using namespace data_structure;

using key_type = uint64_t;
using value_type = uint64_t;

using numa_allocator_type = numa::numa_allocator<void>;
using metall_manager_type = metall::basic_manager<uint32_t, 1 << 21, numa_allocator_type>;

using local_adjacency_list_type =  multithread_adjacency_list<key_type,
                                                              value_type,
                                                              typename metall_manager_type::allocator_type<void>>;
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
    std::vector<metall_manager_type *> managers;
    for (const auto &file_name : option.segment_file_name_list) {
      managers.emplace_back(new metall_manager_type(metall::create_only,
                                                    file_name.c_str(),
                                                    option.segment_size,
                                                    numa_allocator_type()));
    }

    auto adj_list = adjacency_list_type(option.adj_list_key_name, managers.begin(), managers.end());

    run_bench(option, numa_aware_bench, &adj_list);

    const auto start = metall::detail::utility::elapsed_time_sec();
    for (auto manager : managers) {
      manager->sync();
    }
    const auto elapsed_time = metall::detail::utility::elapsed_time_sec(start);
    std::cout << "sync_time (s)\t" << elapsed_time << std::endl;

    std::cout << "Writing profile" << std::endl;
    for (int i = 0; i < (int)option.segment_file_name_list.size(); ++i) {
      std::cout << "-------------------- [" << i << "] --------------------" << std::endl;
      managers[i]->profile(&(std::cout));
    }
  }

  return 0;
}