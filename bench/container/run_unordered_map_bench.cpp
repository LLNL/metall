// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks the STL map container using different allocators.
/// Usage:
/// ./run_map_bench
/// # modify the values in the main(), if needed.

#include <iostream>
#include <unordered_map>
#include <boost/unordered_map.hpp>
#include <metall/container/unordered_map.hpp>
#include <metall/metall.hpp>
#include <metall/detail/time.hpp>

#include "bench_common.hpp"

int main() {
  std::size_t scale = 17;
  std::size_t num_inputs = (1ULL << scale) * 16;
  std::vector<std::pair<uint64_t, uint64_t>> inputs;

  // gen_edges(scale, num_inputs, inputs);
  gen_random_values(num_inputs, inputs);
  std::cout << "Generated inputs\t" << inputs.size() << std::endl;

  {
    std::unordered_map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "unordered_map took (s)\t" << elapsed_time << std::endl;
  }

  {
    boost::unordered_map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost unordered_map took (s)\t" << elapsed_time << std::endl;
  }

  {
    metall::manager mngr(metall::create_only, "/tmp/metall");
    metall::container::unordered_map<uint64_t, uint64_t> map(
        mngr.get_allocator());

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv : inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost unordered_map with Metall took (s)\t" << elapsed_time
              << std::endl;
  }

  return 0;
}