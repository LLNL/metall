// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
#define METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP

#include <iostream>

#include <metall/detail/utility/time.hpp>
#include <metall/detail/utility/memory.hpp>
#include <metall_utility/open_mp.hpp>

namespace adjacency_list_bench {

namespace {
namespace util = metall::detail::utility;
namespace omp = metall_utility::omp;
}

#ifdef SMALL_ALLOCATION_TEST
constexpr std::size_t k_chunk_size = 1ULL << 10ULL;
#else
constexpr std::size_t k_chunk_size = 1ULL << 26ULL;
#endif

inline void print_current_num_page_faults() {
  const auto page_faults = util::get_num_page_faults();
  std::cout << "#of page faults (minflt majflt)\t" << page_faults.first << "\t" << page_faults.second << std::endl;
}

inline void print_omp_configuration() {
  OMP_DIRECTIVE(parallel)
  {
    OMP_DIRECTIVE(single)
    {
      std::cout << "Run with " << omp::get_num_threads() << " threads" << std::endl;
      const auto ret = omp::get_schedule();
      std::cout << "kind " << omp::schedule_kind_name(ret.first)
                << ", chunk_size " << ret.second << std::endl;
    }
  }
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
    const auto start = util::elapsed_time_sec();
    OMP_DIRECTIVE(parallel for schedule (runtime))
    for (std::size_t i = 0; i < key_value_list.size(); ++i) {
      adj_list->add(key_value_list[i].first, key_value_list[i].second);
    }
    const auto elapsed_time = util::elapsed_time_sec(start);

    std::cout << "#of inserted elements\t" << key_value_list.size() << std::endl;
    std::cout << "Elapsed time (s)" << "\t" << elapsed_time << std::endl;
    std::cout << "DRAM usage (GB)" << "\t" << (double)util::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
    std::cout << "DRAM cache usage (GB)" << "\t" << (double)util::get_page_cache_size() / (1ULL << 30ULL) << std::endl;
    print_current_num_page_faults();

    total_elapsed_time += elapsed_time;

    if (itr == end) break;
    ++count_loop;
  }

  return total_elapsed_time;
}

}

#endif //METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
