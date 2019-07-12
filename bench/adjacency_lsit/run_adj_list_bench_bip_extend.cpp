// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <vector>

#include <boost/interprocess/managed_external_buffer.hpp>
#include <boost/interprocess/segment_manager.hpp>
#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>

#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

namespace bip = boost::interprocess;
using manager_type = bip::managed_external_buffer;
using allocator_type = bip::allocator<void, typename manager_type::segment_manager>;
using adjacency_list_type =  data_structure::multithread_adjacency_list<key_type, value_type, allocator_type>;

void *map_file(const std::string &backing_file_name, const size_t file_size) {
  metall::detail::utility::create_file(backing_file_name);
  metall::detail::utility::extend_file_size(backing_file_name, file_size);

  auto ret = metall::detail::utility::map_file_write_mode(backing_file_name,
                                                          nullptr,
                                                          file_size, 0);
  if (ret.first == -1) {
    std::cerr << "Failed to map a file" << std::endl;
    std::abort();
  }

  ::close(ret.first);

  return ret.second;
}

void *map_anonymous(const size_t file_size) {
  void *ret = metall::detail::utility::os_mmap(nullptr,
                                               file_size,
                                               PROT_READ | PROT_WRITE,
                                               MAP_PRIVATE | MAP_ANONYMOUS,
                                               -1, 0);
  if (!ret) {
    std::cerr << "Failed to map an anonymous region" << std::endl;
    std::abort();
  }
  return ret;
}

int main(int argc, char *argv[]) {
  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.segment_size == 0) {
    std::cerr << "file size is required" << std::endl;
    std::abort();
  }

  void *addr = nullptr;
  if (option.segment_file_name_list.empty()) {
    std::cout << "!!! Map ANONYMOUS region !!!" << std::endl;
    addr = map_anonymous(option.segment_size);
  } else {
    std::cout << "Map a file" << std::endl;
    bip::file_mapping::remove(option.segment_file_name_list[0].c_str());
    addr = map_file(option.segment_file_name_list[0], option.segment_size);
  }

  {
    manager_type manager(bip::create_only, addr, option.segment_size);
    auto adj_list = manager.construct<adjacency_list_type>(option.adj_list_key_name.c_str())
                                                          ([&addr, &option]() {
                                                             metall::detail::utility::os_msync(addr, option.segment_size);
                                                           },
                                                           manager.get_allocator<void>());

    run_bench(option, adj_list);

    const auto start = utility::elapsed_time_sec();
    metall::detail::utility::os_msync(addr, option.segment_size);
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "sync_time (s)\t" << elapsed_time << std::endl;

    std::cout << "Segment usage (GB) "
              << static_cast<double>(manager.get_size() - manager.get_free_memory()) / (1ULL << 30) << std::endl;
    metall::detail::utility::munmap(addr, option.segment_size, !option.segment_file_name_list.empty());
  }

  return 0;
}