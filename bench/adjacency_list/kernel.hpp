// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
#define METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP

#include <iostream>

#include <metall/detail/time.hpp>
#include <metall/detail/memory.hpp>
#include <metall/utility/open_mp.hpp>

namespace adjacency_list_bench {

namespace {
namespace mdtl = metall::mtlldetail;
namespace omp = metall::utility::omp;
}  // namespace

inline void print_current_num_page_faults() {
  const auto page_faults = mdtl::get_num_page_faults();
  std::cout << "#of page faults (minflt majflt)\t" << page_faults.first << "\t"
            << page_faults.second << std::endl;
}

inline void print_omp_configuration() {
  OMP_DIRECTIVE(parallel) {
    OMP_DIRECTIVE(single) {
      std::cout << "Run with " << omp::get_num_threads() << " threads"
                << std::endl;
      const auto ret = omp::get_schedule();
      std::cout << "kind " << omp::schedule_kind_name(ret.first)
                << ", chunk_size " << ret.second << std::endl;
    }
  }
}

template <typename adjacency_list_type>
using key_value_input_storage_t = std::vector<
    std::vector<std::pair<typename adjacency_list_type::key_type,
                          typename adjacency_list_type::value_type>>>;

template <typename adjacency_list_type>
inline auto allocate_key_value_input_storage() {
  int num_threads = 0;
  OMP_DIRECTIVE(parallel) {
    OMP_DIRECTIVE(single) { num_threads = omp::get_num_threads(); }
  }

  return key_value_input_storage_t<adjacency_list_type>(num_threads);
}

template <typename adjacency_list_type>
inline auto ingest_key_values(
    const key_value_input_storage_t<adjacency_list_type> &input,
    const std::function<void()> &preprocess,
    const std::function<void()> &postprocess,
    adjacency_list_type *const adj_list, const bool verbose = false) {
  if (verbose) print_current_num_page_faults();

  if (preprocess) {
    if (verbose) std::cout << "----- Pre-process -----" << std::endl;
    const auto start = mdtl::elapsed_time_sec();
    preprocess();
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    if (verbose)
      std::cout << "Pre-process time (s)\t" << elapsed_time << std::endl;
  }

  if (verbose) std::cout << "----- Ingest Main -----" << std::endl;
  // Ingest edges main
  std::size_t num_inserted = 0;
  const auto ingest_start = mdtl::elapsed_time_sec();
  OMP_DIRECTIVE(parallel reduction(+ : num_inserted)) {
    assert((int)input.size() == (int)omp::get_num_threads());
    const auto &key_value_list = input.at(omp::get_thread_num());
    for (std::size_t i = 0; i < key_value_list.size(); ++i) {
      adj_list->add(key_value_list[i].first, key_value_list[i].second);
    }
    num_inserted += key_value_list.size();
  }
  const auto ingest_elapsed_time = mdtl::elapsed_time_sec(ingest_start);

  if (verbose) {
    std::cout << "#of inserted elements\t" << num_inserted << std::endl;
    std::cout << "Ingest elapsed time (s)\t" << ingest_elapsed_time
              << std::endl;
    std::cout << "DRAM usage (GB)\t"
              << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL)
              << std::endl;
    std::cout << "DRAM cache usage (GB)\t"
              << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
              << std::endl;
    print_current_num_page_faults();
  }

  if (postprocess) {
    if (verbose) std::cout << "----- Post-process -----" << std::endl;
    const auto start = mdtl::elapsed_time_sec();
    postprocess();
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    if (verbose)
      std::cout << "Post-process time (s)\t" << elapsed_time << std::endl;
  }

  return ingest_elapsed_time;
}
}  // namespace adjacency_list_bench

#endif  // METALL_BENCH_ADJACENCY_LIST_KERNEL_HPP
