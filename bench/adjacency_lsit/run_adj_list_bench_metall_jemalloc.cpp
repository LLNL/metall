// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// \brief Use jemalloc as the allocator of the internal data structures
// JEMALLOC_ROOT option must be given to cmake to build this program.
// $ ls ${JEMALLOC_ROOT}
// bin/     include/ lib/     share/
// Just compile and install jemalloc to get the files

#include <iostream>
#include <string>
#include <vector>

#include <metall/manager.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"
#include "../utility/time.hpp"
#include "../utility/jemalloc_allocator.hpp"

using namespace adjacency_list_bench;

using manager_type = metall::v0::manager_v0<uint32_t, 1 << 21, utility::jemalloc_allocator<void>>;

using key_type = uint64_t;
using value_type = uint64_t;
using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type,
                                                                        value_type,
                                                                        typename manager_type::allocator_type<void>>;

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
    manager_type manager(metall::create_only,
                         option.segment_file_name.c_str(),
                         option.segment_size);
    auto adj_list = manager.construct<adjacency_list_type>(option.adj_list_key_name.c_str())(manager.get_allocator<>());

    run_bench(option, adj_list);

    const auto start = utility::elapsed_time_sec();
    manager.sync();
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "sync_time (s)\t" << elapsed_time << std::endl;

    std::cout << "Writing profile" << std::endl;
    manager.profile("/tmp/metall_profile.log");
  }

  return 0;
}