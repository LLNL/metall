// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <memory>

#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using adj_list_t = data_structure::multithread_adjacency_list<
    key_type, value_type, typename metall::manager::allocator_type<std::byte>>;

int main(int argc, char *argv[]) {
  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.datastore_path_list.empty()) {
    std::cerr << "Datastore path is required" << std::endl;
    std::abort();
  }

  // Stage in
  if (option.append && !option.staging_location.empty()) {
    const auto start = metall::mtlldetail::elapsed_time_sec();
    metall::manager::copy(option.datastore_path_list[0].c_str(),
                          option.staging_location.c_str());
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "\nStage in took (s)\t" << elapsed_time << std::endl;
  }

  std::unique_ptr<metall::manager> manager;

  // Bench main
  {
    const auto data_store_path = (option.staging_location.empty())
                                     ? option.datastore_path_list[0]
                                     : option.staging_location;
    manager = (option.append)
                  ? std::make_unique<metall::manager>(metall::open_only,
                                                      data_store_path.c_str())
                  : std::make_unique<metall::manager>(metall::create_only,
                                                      data_store_path.c_str());

    auto adj_list =
        (option.append)
            ? manager->find<adj_list_t>(option.adj_list_key_name.c_str()).first
            : manager->construct<adj_list_t>(option.adj_list_key_name.c_str())(
                  manager->get_allocator<>());
    run_bench(option, adj_list);
  }

  // Flush data
  {
    const auto start = metall::mtlldetail::elapsed_time_sec();
    manager->flush();
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "Flushing data took (s)\t" << elapsed_time << std::endl;
  }

  // Close Metall
  {
    const auto start = metall::mtlldetail::elapsed_time_sec();
    manager.reset(nullptr);
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "Closing Metall took (s)\t" << elapsed_time << std::endl;
  }

  // Stage out
  if (!option.staging_location.empty()) {
    const auto start = metall::mtlldetail::elapsed_time_sec();
    metall::manager::copy(option.staging_location.c_str(),
                          option.datastore_path_list[0].c_str());
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "Stage out took (s)\t" << elapsed_time << std::endl;
  }

  return 0;
}