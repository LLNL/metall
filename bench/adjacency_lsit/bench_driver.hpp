// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_ADJACENCY_LIST_BENCH_DRIVER_HPP
#define METALL_BENCH_ADJACENCY_LIST_BENCH_DRIVER_HPP

#include <unistd.h>
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <boost/algorithm/string.hpp>

#include "edge_generator/rmat_edge_generator.hpp"
#include "../utility/pair_reader.hpp"
#include "kernel.hpp"
#include "numa_aware_kernel.hpp"

namespace adjacency_list_bench {
// ---------------------------------------- //
// Option
// ---------------------------------------- //
struct bench_options {
  std::vector<std::string> segment_file_name_list;
  std::string adj_list_key_name{"adj_list"};

  std::size_t segment_size{1 << 25};
  std::vector<std::string> input_file_name_list;

  uint64_t seed{123};
  uint64_t vertex_scale{17};
  uint64_t edge_count{(1ULL << 17ULL) * 16};
  double a{0.57};
  double b{0.19};
  double c{0.19};
  bool scramble_id{true};
  bool undirected{true};

  std::string dump_file_name;
};

inline bool parse_options(int argc, char **argv, bench_options *option) {
  int p;
  while ((p = ::getopt(argc, argv, "o:k:f:s:v:e:a:b:c:r:u:d:")) != -1) {
    switch (p) {
      case 'o': {
        option->segment_file_name_list.clear();
        boost::split(option->segment_file_name_list, optarg, boost::is_any_of(":"));
        break;
      }

      case 'k': option->adj_list_key_name = optarg;
        break;

      case 'f':option->segment_size = std::stoull(optarg);
        break;

      case 's':option->seed = std::stoull(optarg);
        break;

      case 'v':option->vertex_scale = std::stoull(optarg);
        break;

      case 'e':option->edge_count = std::stoull(optarg);
        break;

      case 'a':option->a = std::stod(optarg);
        break;

      case 'b':option->b = std::stod(optarg);
        break;

      case 'c':option->c = std::stod(optarg);
        break;

      case 'r':option->scramble_id = static_cast<bool>(std::stoi(optarg));
        break;

      case 'u':option->undirected = static_cast<bool>(std::stoi(optarg));
        break;

      case 'd': // dump constructed adjacency list at the end of benchmark
        option->dump_file_name = optarg;
        break;

      default:std::cerr << "Invalid option" << std::endl;
        return false;
    }
  }

  for (int index = optind; index < argc; index++) {
    option->input_file_name_list.emplace_back(argv[index]);
  }

  if (!option->segment_file_name_list.empty()) {
    std::cout << "segment_file_name: " << std::endl;
    for (const auto &name : option->segment_file_name_list) {
      std::cout << " " << name << std::endl;
    }
  }
  std::cout << "segment_size: " << option->segment_size << std::endl;

  if (option->input_file_name_list.empty()) {
    std::cout << "adj_list_key_name" << option->adj_list_key_name
              << "\nseed: " << option->seed
              << "\nvertex_scale: " << option->vertex_scale
              << "\nedge_count: " << option->edge_count
              << "\na: " << option->a
              << "\nb: " << option->b
              << "\nc: " << option->c
              << "\nscramble_id: " << static_cast<int>(option->scramble_id)
              << "\nundirected: " << static_cast<int>(option->undirected) << std::endl;
  } else {
    std::cout << "Input file list:" << std::endl;
    for (const auto &str : option->input_file_name_list) {
      std::cout << str << std::endl;
    }
  }

  return true;
}

// ---------------------------------------- //
// Benchmark drivers
// ---------------------------------------- //
struct single_numa_bench_t {};
[[maybe_unused]] static const single_numa_bench_t single_numa_bench{};

struct numa_aware_bench_t {};
[[maybe_unused]] static const numa_aware_bench_t numa_aware_bench{};

/// \brief Run benchmark reading files that contain key-value data, such as edge list
template <typename adjacency_list_type>
inline double run_bench_kv_file(const std::vector<std::string> &input_file_name_list,
                                single_numa_bench_t,
                                adjacency_list_type *adj_list) {

  utility::pair_reader<typename adjacency_list_type::key_type, typename adjacency_list_type::value_type>
      reader(input_file_name_list.begin(), input_file_name_list.end());

  return kernel(reader.begin(), reader.end(), adj_list);
}

/// \brief Run benchmark reading files that contain key-value data, such as edge list.
/// Uses the NUMA-aware benchmark kernel
template <typename adjacency_list_type>
inline double run_bench_kv_file(const std::vector<std::string> &input_file_name_list,
                                numa_aware_bench_t,
                                adjacency_list_type *adj_list) {

  utility::pair_reader<typename adjacency_list_type::key_type, typename adjacency_list_type::value_type>
      reader(input_file_name_list.begin(), input_file_name_list.end());

  return numa_aware_kernel(reader.begin(), reader.end(), adj_list);
}

/// \brief Run benchmark generating an rmat graph
template <typename adjacency_list_type>
inline double run_bench_rmat_edge(const uint32_t seed, const uint64_t vertex_scale, const uint64_t edge_count,
                                  const double a, const double b, const double c,
                                  const bool scramble_id, const bool undirected,
                                  single_numa_bench_t,
                                  adjacency_list_type *adj_list) {

  edge_generator::rmat_edge_generator rmat(seed, vertex_scale, edge_count, a, b, c, scramble_id, undirected);
  return kernel(rmat.begin(), rmat.end(), adj_list);
}

/// \brief Run benchmark generating an rmat graph
/// Uses the NUMA-aware benchmark kernel
template <typename adjacency_list_type>
inline double run_bench_rmat_edge(const uint32_t seed, const uint64_t vertex_scale, const uint64_t edge_count,
                                  const double a, const double b, const double c,
                                  const bool scramble_id, const bool undirected,
                                  numa_aware_bench_t,
                                  adjacency_list_type *adj_list) {

  edge_generator::rmat_edge_generator rmat(seed, vertex_scale, edge_count, a, b, c, scramble_id, undirected);
  return numa_aware_kernel(rmat.begin(), rmat.end(), adj_list);
}

template <typename adjacency_list_type>
inline void dump_adj_list(const adjacency_list_type &adj_list, const std::string &file_name) {
  std::cout << "Dumping adjacency list..." << std::endl;

  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    std::cerr << "Cannot open " << file_name << std::endl;
    return;
  }

  for (auto key_itr = adj_list.keys_begin(), key_end = adj_list.keys_end();
       key_itr != key_end; ++key_itr) {
    for (auto value_itr = adj_list.values_begin(key_itr->first), value_end = adj_list.values_end(key_itr->first);
         value_itr != value_end; ++value_itr) {
      ofs << key_itr->first << " " << *value_itr << "\n";
    }
  }
  ofs.close();

  std::cout << "Finished" << std::endl;
};

template <typename bench_mode_type, typename adjacency_list_type>
void run_bench(const bench_options &options, bench_mode_type, adjacency_list_type *adj_list) {

  double elapsed_time_sec;
  if (options.input_file_name_list.size() > 0) {
    std::cout << "Get inputs from key-value files" << std::endl;
    elapsed_time_sec = run_bench_kv_file(options.input_file_name_list, bench_mode_type(), adj_list);
  } else {
    std::cout << "Get inputs from the RMAT edge generator" << std::endl;
    elapsed_time_sec = run_bench_rmat_edge(options.seed, options.vertex_scale, options.edge_count,
                                           options.a, options.b, options.c,
                                           options.scramble_id, options.undirected, bench_mode_type(), adj_list);
  }
  std::cout << "Finished adj_list (s)\t" << elapsed_time_sec << std::endl;

  if (!options.dump_file_name.empty()) {
    dump_adj_list(*adj_list, options.dump_file_name);
  }
}

}
#endif //METALL_BENCH_ADJACENCY_LIST_BENCH_DRIVER_HPP
