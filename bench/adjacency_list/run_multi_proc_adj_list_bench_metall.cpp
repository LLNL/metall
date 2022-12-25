// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <mpi.h>

#include <iostream>
#include <string>
#include <memory>

#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include <metall/utility/metall_mpi_adaptor.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;
using namespace metall::utility;

using key_type = uint64_t;
using value_type = uint64_t;

using adj_list_t = data_structure::multithread_adjacency_list<key_type,
                                                              value_type,
                                                              typename metall::manager::allocator_type<std::byte>>;

int main(int argc, char *argv[]) {

  MPI_Init(&argc, &argv);
  {
    const auto rank = mpi::comm_rank(MPI_COMM_WORLD);

    bench_options option;
    if (!parse_options(argc, argv, &option)) {
      std::abort();
    }

    if (option.datastore_path_list.empty()) {
      std::cerr << "Datastore path is required" << std::endl;
      std::abort();
    }

    std::unique_ptr<metall_mpi_adaptor> global_manager;

    // Bench main
    {
      const auto data_store_path = (option.staging_location.empty())
                                   ? option.datastore_path_list[0] : option.staging_location;
      global_manager = (option.append) ? std::make_unique<metall_mpi_adaptor>(metall::open_only,
                                                                       data_store_path.c_str(),
                                                                       MPI_COMM_WORLD)
                                : std::make_unique<metall_mpi_adaptor>(metall::create_only,
                                                                       data_store_path.c_str(),
                                                                       MPI_COMM_WORLD);
      auto &local_manager = global_manager->get_local_manager();
      auto adj_list = (option.append) ? local_manager.find<adj_list_t>(option.adj_list_key_name.c_str()).first
                                      : local_manager.construct<adj_list_t>(option.adj_list_key_name.c_str())(
              local_manager.get_allocator());
      run_bench(option, adj_list);
    }
    mpi::barrier(MPI_COMM_WORLD);

    // Flush data
    {
      const auto start = metall::mtlldetail::elapsed_time_sec();
      global_manager->get_local_manager().flush();
      const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
      std::cout << "Flushing data took (s)\t" << elapsed_time << std::endl;
    }
    mpi::barrier(MPI_COMM_WORLD);

    // Close Metall
    {
      const auto start = metall::mtlldetail::elapsed_time_sec();
      global_manager.reset(nullptr);
      const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
      std::cout << "Closing Metall took (s)\t" << elapsed_time << std::endl;
    }
    mpi::barrier(MPI_COMM_WORLD);
  }

  return 0;
}