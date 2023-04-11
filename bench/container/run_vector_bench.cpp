// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks the STL map container using different allocators.
/// Usage:
/// ./run_map_bench
/// # modify the values in the main(), if needed.

#include <iostream>
#include <vector>
#include <boost/container/vector.hpp>
#include <metall/container/vector.hpp>
#include <metall/metall.hpp>
#include <metall/detail/time.hpp>

#include "bench_common.hpp"

int main() {
  std::size_t scale = 22;
  std::size_t num_inputs = (1ULL << scale) * 16;
  std::vector<std::pair<uint64_t, uint64_t>> inputs;

  gen_random_values(num_inputs, inputs);
  std::cout << "Generated inputs\t" << inputs.size() << std::endl;

  {
    std::cout << "vector (push_back)" << std::endl;
    std::vector<std::pair<uint64_t, uint64_t>> vec;
    run_bench(inputs, [&vec](const auto &kv) { vec.push_back(kv); });
  }

  {
    std::cout << "Boost vector (push_back)" << std::endl;
    boost::container::vector<std::pair<uint64_t, uint64_t>> vec;
    run_bench(inputs, [&vec](const auto &kv) { vec.push_back(kv); });
  }

  {
    std::cout << "Boost vector (push_back) with Metall" << std::endl;
    metall::manager mngr(metall::create_only, "/tmp/metall");
    metall::container::vector<std::pair<uint64_t, uint64_t>> vec(
        mngr.get_allocator());
    run_bench(inputs, [&vec](const auto &kv) { vec.push_back(kv); });
  }
  std::cout << std::endl;

  {
    std::cout << "vector ([])" << std::endl;
    std::vector<std::pair<uint64_t, uint64_t>> vec;
    std::size_t index = 0;
    run_bench(
        inputs, [&vec, &inputs]() { vec.resize(inputs.size()); },
        [&vec, &index](const auto &kv) { vec[index++] = kv; });
  }

  {
    std::cout << "Boost ([]) vector" << std::endl;
    boost::container::vector<std::pair<uint64_t, uint64_t>> vec;
    std::size_t index = 0;
    run_bench(
        inputs, [&vec, &inputs]() { vec.resize(inputs.size()); },
        [&vec, &index](const auto &kv) { vec[index++] = kv; });
  }

  {
    std::cout << "Boost vector ([]) with Metall" << std::endl;
    metall::manager mngr(metall::create_only, "/tmp/metall");
    metall::container::vector<std::pair<uint64_t, uint64_t>> vec(
        mngr.get_allocator());
    std::size_t index = 0;
    run_bench(
        inputs, [&vec, &inputs]() { vec.resize(inputs.size()); },
        [&vec, &index](const auto &kv) { vec[index++] = kv; });
  }
  return 0;
}