// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cstddef>

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <metall/detail/time.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

namespace bip = boost::interprocess;
using allocator_type =
    bip::allocator<std::byte, bip::managed_mapped_file::segment_manager>;
using adjacency_list_type =
    data_structure::multithread_adjacency_list<key_type, value_type,
                                               allocator_type>;

namespace mdtl = metall::mtlldetail;

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
    // bip::file_mapping::remove(option.segment_file_name.c_str());

    bip::managed_mapped_file mfile(bip::create_only,
                                   option.datastore_path_list[0].c_str(),
                                   option.segment_size);
    auto adj_list = mfile.construct<adjacency_list_type>(
        option.adj_list_key_name.c_str())(mfile.get_allocator<std::byte>());
    run_bench(option, adj_list);

    const auto start = mdtl::elapsed_time_sec();
    mfile.flush();  // NOTE: this method calls msync() with MS_ASYNC
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Async sync_time (s)\t" << elapsed_time << std::endl;

    std::cout << "Segment usage (GB)\t"
              << static_cast<double>(mfile.get_size() -
                                     mfile.get_free_memory()) /
                     (1ULL << 30)
              << std::endl;
  }

  return 0;
}