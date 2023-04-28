// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP
#define METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP

#include <metall/detail/utilities.hpp>
#include <metall/detail/time.hpp>
#include "../utility/numa.hpp"
#include "kernel.hpp"

namespace adjacency_list_bench {

namespace {
namespace mdtl = metall::mtlldetail;
}

inline void configure_numa() {
#ifdef _OPENMP

  int num_threads = 0;

#pragma omp parallel
  {
    if (omp::get_thread_num() == 0) {
      num_threads = omp::get_num_threads();
      std::cout << "#threads\t" << num_threads << std::endl;
      std::cout << "#numa nodes\t" << bench_utility::numa::get_avail_nodes()
                << std::endl;
      if (num_threads < bench_utility::numa::get_avail_nodes()) {
        std::cerr << "#threads must be equal or larger than #nodes"
                  << std::endl;
        std::abort();
      }
    }
  }

  for (int t = 0; t < num_threads; ++t) {
    bench_utility::numa::set_node(t);
  }
#endif
}

template <typename adjacency_list_type, typename input_iterator>
inline double numa_aware_kernel(const std::size_t chunk_size,
                                input_iterator itr, input_iterator end,
                                adjacency_list_type *const adj_list) {
  configure_numa();

  print_omp_configuration();

  std::vector<typename input_iterator::value_type> key_value_list;
  size_t count_loop(0);
  double total_elapsed_time = 0.0;
  while (true) {
    std::cout << "\n[ " << count_loop << " ]" << std::endl;

    key_value_list.clear();
    while (itr != end && key_value_list.size() < chunk_size) {
      key_value_list.emplace_back(*itr);
      ++itr;
    }

    print_current_num_page_faults();
    const auto start = mdtl::elapsed_time_sec();
    OMP_DIRECTIVE(parallel) {
      const auto thread_id = omp::get_thread_num();
      const auto num_threads = omp::get_num_threads();
      const auto local_thread_num =
          bench_utility::numa::get_local_thread_num(thread_id);
      const auto num_local_threads =
          bench_utility::numa::get_local_num_threads(thread_id, num_threads);
      const auto range = mdtl::partial_range(
          key_value_list.size(), local_thread_num, num_local_threads);

      for (std::size_t i = range.first; i < range.second; ++i) {
        // If the key is assigned to the local numa node, add to the adjacency
        // list
        if (adj_list->partition_index(key_value_list[i].first) ==
            bench_utility::numa::get_node(thread_id)) {
          adj_list->add(key_value_list[i].first, key_value_list[i].second);
        }
      }
    }
    adj_list->sync();
    const auto elapsed_time = mdtl::elapsed_time_sec(start);

    std::cout << "#of inserted elements\t" << key_value_list.size()
              << std::endl;
    std::cout << "Elapsed time including sync (s)"
              << "\t" << elapsed_time << std::endl;
    std::cout << "DRAM usage(gb)"
              << "\t"
              << static_cast<double>(mdtl::get_used_ram_size()) /
                     (1ULL << 30ULL)
              << std::endl;
    print_current_num_page_faults();

    total_elapsed_time += elapsed_time;

    if (itr == end) break;
    ++count_loop;
  }

  return total_elapsed_time;
}

}  // namespace adjacency_list_bench

#endif  // METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP
