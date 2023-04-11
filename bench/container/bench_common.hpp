// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_CONTAINER_BENCH_COMMON_HPP
#define METALL_BENCH_CONTAINER_BENCH_COMMON_HPP

#include <vector>
#include <utility>
#include <random>

#include <metall/utility/random.hpp>

#include "../bench/adjacency_list/edge_generator/rmat_edge_generator.hpp"

namespace mdtl = metall::mtlldetail;

using rmat_generator_type = edge_generator::rmat_edge_generator;

void gen_edges(const std::size_t vertex_scale, const std::size_t num_edges,
               std::vector<std::pair<uint64_t, uint64_t>> &buf) {
  rmat_generator_type rmat_generator(123, vertex_scale, num_edges, 0.57, 0.19,
                                     0.19, true, false);

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

template <typename input_container_type, typename inserter_func>
void run_bench(const input_container_type &inputs,
               const inserter_func &inserter) {
  const auto start = mdtl::elapsed_time_sec();
  for (const auto &kv : inputs) {
    inserter(kv);
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << "Took (s)\t" << elapsed_time << std::endl;
}

template <typename input_container_type, typename preprocessor_func,
          typename inserter_func>
void run_bench(const input_container_type &inputs,
               const preprocessor_func &preprocessor,
               const inserter_func &inserter) {
  const auto start = mdtl::elapsed_time_sec();
  preprocessor();
  for (const auto &kv : inputs) {
    inserter(kv);
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << "Took (s)\t" << elapsed_time << std::endl;
}

#endif  // METALL_BENCH_CONTAINER_BENCH_COMMON_HPP
