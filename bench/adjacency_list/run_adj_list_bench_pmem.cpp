// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cstddef>

#include <pmem_allocator.h>

#include <metall/detail/utility/time.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using allocator_type = libmemkind::pmem::allocator<std::byte>;
using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type, value_type, allocator_type>;

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.segment_file_name_list.empty()) {
    std::cerr << "Segment file name is required" << std::endl;
    std::abort();
  }

  allocator_type allocator(option.segment_file_name_list[0].c_str(), option.segment_size);
  adjacency_list_type adj_list(allocator);
  run_bench(option, single_numa_bench, &adj_list);

  return 0;
}