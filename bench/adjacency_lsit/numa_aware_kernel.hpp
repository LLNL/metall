// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP
#define METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP

#include <metall/detail/utility/common.hpp>
#include "kernel.hpp"
#include "../utility/numa.hpp"

namespace adjacency_list_bench {

inline void configure_numa() {
#ifdef _OPENMP

  int num_threads = 0;

#pragma omp parallel
  {
    if (omp::get_thread_num() == 0) {
      num_threads = omp::get_num_threads();
      std::cout << "#threads\t" << num_threads << std::endl;
      std::cout << "#numa nodes\t" << numa::get_avail_nodes() << std::endl;
      if (num_threads < numa::get_avail_nodes()) {
        std::cerr << "#threads must be equal or larger than #nodes" << std::endl;
        std::abort();
      }
    }
  }

  for (int t = 0; t < num_threads; ++t) {
    numa::set_node(t);
  }
#endif
}

template <typename adjacency_list_type, typename input_iterator>
inline double numa_aware_kernel(input_iterator itr, input_iterator end, adjacency_list_type *const adj_list) {

  configure_numa();

  print_omp_configuration();

  std::vector<typename input_iterator::value_type> key_value_list;
  size_t count_loop(0);
  double total_elapsed_time = 0.0;
  while (true) {
    std::cout << "\n[ " << count_loop << " ]" << std::endl;

    key_value_list.clear();
    while (itr != end && key_value_list.size() < k_chunk_size) {
      key_value_list.emplace_back(*itr);
      ++itr;
    }

    print_current_num_page_faults();
    const auto start = utility::elapsed_time_sec();
#ifdef _OPENMP
#pragma omp parallel
#endif
    {
      const auto thread_id = omp::get_thread_num();
      const auto num_threads = omp::get_num_threads();
      const auto local_thread_num = numa::get_local_thread_num(thread_id);
      const auto num_local_threads = numa::get_local_num_threads(thread_id, num_threads);
      const auto range = metall::detail::utility::partial_range(key_value_list.size(),
                                                                local_thread_num, num_local_threads);

      for (std::size_t i = range.first; i < range.second; ++i) {
        // If the key is assigned to the local numa node, add to the adjacency list
        if (adj_list->partition_index(key_value_list[i].first) == numa::get_node(thread_id)) {
          adj_list->add(key_value_list[i].first, key_value_list[i].second);
        }
      }
    }
    adj_list->sync();
    const auto elapsed_time = utility::elapsed_time_sec(start);

    std::cout << "#of inserted elements\t" << key_value_list.size() << std::endl;
    std::cout << "Elapsed time including sync (s)" << "\t" << elapsed_time << std::endl;
    std::cout << "DRAM usage(gb)" << "\t"
              << static_cast<double>(utility::get_used_ram_size()) / (1ULL << 30ULL) << std::endl;
    print_current_num_page_faults();

    total_elapsed_time += elapsed_time;

    if (itr == end) break;
    ++count_loop;
  }

  return total_elapsed_time;
}

}

#endif //METALL_BENCH_ADJACENCY_LIST_NUMA_AWARE_KERNEL_HPP
