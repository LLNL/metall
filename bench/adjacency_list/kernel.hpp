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
namespace omp = metall::utility::omp;
}

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

template <typename adjacency_list_type>
using key_value_input_storage_t = std::vector<std::vector<std::pair<typename adjacency_list_type::key_type,
                                                                    typename adjacency_list_type::value_type>>>;

template <typename adjacency_list_type>
inline auto allocate_key_value_input_storage() {
  int num_threads = 0;
  OMP_DIRECTIVE(parallel)
  {
    OMP_DIRECTIVE(single)
    {
      num_threads = omp::get_num_threads();
    }
  }

  return key_value_input_storage_t<adjacency_list_type>(num_threads);
}

using closing_function_type = std::function<void()>;

template <typename adjacency_list_type>
inline auto ingest_key_values(const key_value_input_storage_t<adjacency_list_type> &input,
                              closing_function_type closing_function,
                              adjacency_list_type *const adj_list) {
  print_current_num_page_faults();

  std::size_t num_inserted = 0;
  const auto start = util::elapsed_time_sec();
  OMP_DIRECTIVE(parallel reduction(+:num_inserted))
  {
    assert((int)input.size() == (int)omp::get_num_threads());
    const auto &key_value_list = input.at(omp::get_thread_num());
    for (std::size_t i = 0; i < key_value_list.size(); ++i) {
      adj_list->add(key_value_list[i].first, key_value_list[i].second);
    }
    num_inserted += key_value_list.size();
  }
  if (closing_function) {
    closing_function();
  }
  const auto elapsed_time = util::elapsed_time_sec(start);

  std::cout << "#of inserted elements\t" << num_inserted << std::endl;
  std::cout << "Elapsed time (s)\t" << elapsed_time << std::endl;
  std::cout << "DRAM usage (GB)\t" << (double)util::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)\t" << (double)util::get_page_cache_size() / (1ULL << 30ULL) << std::endl;
  print_current_num_page_faults();

  return elapsed_time;
}
}

#endif //METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
