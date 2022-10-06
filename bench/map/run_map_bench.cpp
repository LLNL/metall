// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks the STL map container using different allocators.
/// Usage:
/// ./run_map_bench
/// # modify the values in the main(), if needed.

#include <iostream>
#include <utility>
#include <map>
#include <vector>
#include <random>
#include <boost/container/map.hpp>
#include <metall/container/map.hpp>
#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include <metall/utility/random.hpp>
#include "../bench/adjacency_list/edge_generator/rmat_edge_generator.hpp"

namespace mdtl = metall::mtlldetail;

using rmat_generator_type = edge_generator::rmat_edge_generator;

void gen_edges(const std::size_t vertex_scale,
               const std::size_t num_edges,
               std::vector<std::pair<uint64_t, uint64_t>> &buf) {
  rmat_generator_type rmat_generator(123, vertex_scale, num_edges, 0.57, 0.19, 0.19, true, false);

  buf.reserve(num_edges);
  for (auto itr = rmat_generator.begin(); itr != rmat_generator.end(); ++itr) {
    buf.emplace_back(*itr);
  }
}

void gen_random_values(const std::size_t num_values,
                       std::vector<std::pair<uint64_t, uint64_t>> &buf) {
  metall::utility::rand_1024 rnd_generator(std::random_device{}());

  buf.reserve(num_values);
  for (std::size_t i = 0; i < num_values; ++i) {
    buf.emplace_back(rnd_generator(), rnd_generator());
  }
}

int main() {
  std::size_t scale = 17;
  std::size_t num_inputs = (1ULL << scale) * 16;
  std::vector<std::pair<uint64_t, uint64_t>> inputs;

  //gen_edges(scale, num_inputs, inputs);
  gen_random_values(num_inputs, inputs);
  std::cout << "Generated inputs\t" << inputs.size() << std::endl;

  {
    std::map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv: inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "STL Map took (s)\t" << elapsed_time << std::endl;
  }

  {
    boost::container::map<uint64_t, uint64_t> map;

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv: inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost Map took (s)\t" << elapsed_time << std::endl;
  }

  {
    metall::manager mngr(metall::create_only, "/tmp/metall");
    metall::container::map<uint64_t, uint64_t> map(mngr.get_allocator());

    const auto start = mdtl::elapsed_time_sec();
    for (const auto &kv: inputs) {
      map[kv.first];
      map[kv.second];
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Boost-map with Metall took (s)\t" << elapsed_time << std::endl;
  }

  return 0;
}