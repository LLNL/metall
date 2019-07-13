// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
#define METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP

#include <iostream>

#include "../utility/time.hpp"
#include "../utility/memory.hpp"
#include "../utility/open_mp.hpp"

namespace adjacency_list_bench {
#ifdef SMALL_ALLOCATION_TEST
constexpr std::size_t k_chunk_size = 1ULL << 10ULL;
#else
constexpr std::size_t k_chunk_size = 1ULL << 26ULL;
#endif

inline void print_current_num_page_faults() {
  const auto page_faults = utility::get_num_page_faults();
  std::cout << "#of page faults (minflt majflt)\t" << page_faults.first << "\t" << page_faults.second << std::endl;
}

inline void print_omp_configuration() {
#ifdef _OPENMP
#pragma omp parallel
  {
    if (::omp_get_thread_num() == 0)
      std::cout << "Run with " << ::omp_get_num_threads() << " threads" << std::endl;
  }
  omp_sched_t kind;
  int chunk_size;
  ::omp_get_schedule(&kind, &chunk_size);
  std::cout << "kind " << omp::schedule_kind_name(kind)
            << ", chunk_size " << chunk_size << std::endl;
#else
  std::cout << "Run with a single thread" << std::endl;
#endif
}

template <typename adjacency_list_type, typename input_iterator>
inline double kernel(input_iterator itr, input_iterator end, adjacency_list_type *const adj_list) {
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
#pragma omp parallel for schedule (runtime)
#endif
    for (std::size_t i = 0; i < key_value_list.size(); ++i) {
      adj_list->add(key_value_list[i].first, key_value_list[i].second);
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

#endif //METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
