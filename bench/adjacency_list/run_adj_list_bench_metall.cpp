// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type,
                                                                        value_type,
                                                                        typename metall::manager::allocator_type<std::byte>>;

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.datastore_path_list.empty()) {
    std::cerr << "Datastore path is required" << std::endl;
    std::abort();
  }

  {
    // metall::logger::set_log_level(metall::logger::level::verbose);

    metall::manager manager(metall::create_only, option.datastore_path_list[0].c_str());
    auto adj_list = manager.construct<adjacency_list_type>(option.adj_list_key_name.c_str())(manager.get_allocator<>());

    run_bench(option, adj_list);

    const auto start = metall::mtlldetail::elapsed_time_sec();
    manager.flush();
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "Flushing data took (s)\t" << elapsed_time << std::endl;

    // std::cout << "Writing profile" << std::endl;
    // manager.profile(&(std::cout));
  }

  return 0;
}