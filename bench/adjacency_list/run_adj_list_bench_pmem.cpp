// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cstddef>

#include <pmem_allocator.h>

#include <metall/detail/time.hpp>
#include <metall/detail/file.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using allocator_type = libmemkind::pmem::allocator<std::byte>;
using adjacency_list_type =
    data_structure::multithread_adjacency_list<key_type, value_type,
                                               allocator_type>;

std::string run_command(const std::string &cmd) {
  std::cout << cmd << std::endl;

  const std::string tmp_file("/tmp/tmp_command_result");
  std::string command(cmd + " > " + tmp_file);
  std::system(command.c_str());

  std::ifstream ifs(tmp_file);
  if (!ifs.is_open()) {
    return std::string("Failed to open: " + tmp_file);
  }

  std::string buf;
  buf.assign((std::istreambuf_iterator<char>(ifs)),
             std::istreambuf_iterator<char>());

  metall::mtlldetail::remove_file(tmp_file);

  return buf;
}

int main(int argc, char *argv[]) {
  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.datastore_path_list.empty()) {
    std::cerr << "Datastore path is required" << std::endl;
    std::abort();
  }

  allocator_type allocator(option.datastore_path_list[0].c_str(),
                           option.segment_size);
  adjacency_list_type adj_list(allocator);
  run_bench(option, &adj_list);

  std::cout << "File size\t"
            << metall::mtlldetail::get_file_size(option.datastore_path_list[0])
            << std::endl;
  std::cout << "Actual file size\t"
            << metall::mtlldetail::get_actual_file_size(
                   option.datastore_path_list[0])
            << std::endl;
  std::cout << run_command("df " + option.datastore_path_list[0]) << std::endl;
  std::cout << run_command("du " + option.datastore_path_list[0]) << std::endl;

  return 0;
}