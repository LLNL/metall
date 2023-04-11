// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
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
#include <metall/detail/utilities.hpp>
#include <metall/utility/open_mp.hpp>
#include "edge_generator/rmat_edge_generator.hpp"
#include "../utility/pair_reader.hpp"
#include "kernel.hpp"

namespace adjacency_list_bench {
// ---------------------------------------- //
// Option
// ---------------------------------------- //
#ifdef SMALL_ALLOCATION_TEST
constexpr std::size_t k_default_chunk_size = 1ULL << 10ULL;
#else
constexpr std::size_t k_default_chunk_size = 1ULL << 26ULL;
#endif

struct bench_options {
  std::vector<std::string> datastore_path_list;
  std::string adj_list_key_name{"adj_list"};

  std::size_t chunk_size = k_default_chunk_size;

  std::size_t segment_size{1ULL << 25ULL};
  std::vector<std::string> input_file_name_list;

  struct rmat_option {
    uint64_t seed{123};
    uint64_t vertex_scale{17};
    std::size_t edge_count{(1ULL << 17ULL) * 16};
    double a{0.57};
    double b{0.19};
    double c{0.19};
    bool scramble_id{true};
    bool undirected{true};
  } rmat;

  std::string adj_list_dump_file_name;
  std::string edge_list_dump_file_name;

  bool append{false};  // Append existing data store

  std::string staging_location;  // Copy data store

  bool verbose{false};
};

inline void disp_options(const bench_options &option) {
  std::cout << "adj_list_key_name: " << option.adj_list_key_name << std::endl;
  std::cout << "chunk_size: " << option.chunk_size << std::endl;
  std::cout << "VERBOSE: " << option.verbose << std::endl;

  if (!option.datastore_path_list.empty()) {
    std::cout << "datastore_path_list: " << std::endl;
    for (const auto &name : option.datastore_path_list) {
      std::cout << " " << name << std::endl;
    }
  }
  std::cout << "segment_size (for Boost and pmem) : " << option.segment_size
            << std::endl;

  std::cout << "Append existing data store : "
            << static_cast<int>(option.append) << std::endl;

  std::cout << "Staging location : " << option.staging_location << std::endl;

  if (option.input_file_name_list.empty()) {
    std::cout << "seed: " << option.rmat.seed
              << "\nvertex_scale: " << option.rmat.vertex_scale
              << "\nedge_count: " << option.rmat.edge_count
              << "\na: " << option.rmat.a << "\nb: " << option.rmat.b
              << "\nc: " << option.rmat.c
              << "\nscramble_id: " << static_cast<int>(option.rmat.scramble_id)
              << "\nundirected: " << static_cast<int>(option.rmat.undirected)
              << std::endl;
  } else {
    std::cout << "Input file list:" << std::endl;
    for (const auto &str : option.input_file_name_list) {
      std::cout << str << std::endl;
    }
  }
}

inline auto parse_options(int argc, char **argv, bench_options *option) {
  int p;
  while ((p = ::getopt(argc, argv, "o:k:n:f:s:v:e:a:b:c:r:u:d:D:VAS:")) != -1) {
    switch (p) {
      case 'o':
        option->datastore_path_list.clear();
        boost::split(option->datastore_path_list, optarg,
                     boost::is_any_of(":"));
        break;

      case 'k':
        option->adj_list_key_name = optarg;
        break;

      case 'n':
        option->chunk_size = std::stoull(optarg);
        break;

      case 'f':
        option->segment_size = std::stoull(optarg);
        break;

      case 's':
        option->rmat.seed = std::stoull(optarg);
        break;

      case 'v':
        option->rmat.vertex_scale = std::stoull(optarg);
        break;

      case 'e':
        option->rmat.edge_count = std::stoull(optarg);
        break;

      case 'a':
        option->rmat.a = std::stod(optarg);
        break;

      case 'b':
        option->rmat.b = std::stod(optarg);
        break;

      case 'c':
        option->rmat.c = std::stod(optarg);
        break;

      case 'r':
        option->rmat.scramble_id = static_cast<bool>(std::stoi(optarg));
        break;

      case 'u':
        option->rmat.undirected = static_cast<bool>(std::stoi(optarg));
        break;

      case 'A':
        option->append = true;
        break;

      case 'S':
        option->staging_location = optarg;
        break;

      case 'd':  // dump constructed adjacency list at the end of benchmark
        option->adj_list_dump_file_name = optarg;
        break;

      case 'D':  // dump edge list
        option->edge_list_dump_file_name = optarg;
        break;

      case 'V':
        option->verbose = true;
        break;

      default:
        std::cerr << "Invalid option" << std::endl;
        return false;
    }
  }

  for (int index = optind; index < argc; index++) {
    option->input_file_name_list.emplace_back(argv[index]);
  }

  disp_options(*option);

  return true;
}

// ---------------------------------------- //
// Benchmark drivers
// ---------------------------------------- //
template <typename adjacency_list_type>
inline auto run_bench_kv_file(
    const std::vector<std::string> &input_file_name_list,
    const std::size_t chunk_size, const std::function<void()> &preprocess,
    const std::function<void()> &postprocess, adjacency_list_type *adj_list,
    std::ofstream *const ofs_save_edge, const bool verbose = false) {
  using reader_type =
      bench_utility::pair_reader<typename adjacency_list_type::key_type,
                                 typename adjacency_list_type::value_type>;
  reader_type reader(input_file_name_list.begin(), input_file_name_list.end());

  auto input_storage = allocate_key_value_input_storage<adjacency_list_type>();

  std::size_t count_loop = 0;
  double total_elapsed_time = 0;
  for (auto itr = reader.begin(), end = reader.end(); itr != end;) {
    if (verbose) std::cout << "\n[ " << count_loop << " ]" << std::endl;

    for (auto &input_list : input_storage) input_list.clear();

    std::size_t count_read = 0;
    while (itr != end && count_read < chunk_size) {
      input_storage[count_read % omp::get_num_threads()].emplace_back(*itr);
      ++itr;
      ++count_read;
    }

    if (count_read == 0) break;

    total_elapsed_time += ingest_key_values(input_storage, preprocess,
                                            postprocess, adj_list, verbose);

    if (ofs_save_edge) {
      for (const auto &list : input_storage) {
        for (const auto &elem : list) {
          *ofs_save_edge << elem.first << "\t" << elem.second << "\n";
        }
      }
    }

    ++count_loop;
  }
  ofs_save_edge->close();

  return total_elapsed_time;
}

/// \brief Run benchmark generating an rmat graph
/// If directed_graph is false, the total number of edges to be generated is
/// num_edges x 2
template <typename adjacency_list_type>
inline auto run_bench_rmat_edge(const bench_options::rmat_option &rmat_option,
                                const std::size_t chunk_size,
                                const std::function<void()> &preprocess,
                                const std::function<void()> &postprocess,
                                adjacency_list_type *adj_list,
                                std::ofstream *const ofs_save_edge,
                                const bool verbose = false) {
  // -- Initialize rmat edge generators -- //
  using rmat_generator = edge_generator::rmat_edge_generator;
  std::vector<std::unique_ptr<rmat_generator>> generator_list;
  std::vector<typename rmat_generator::iterator> generator_itr_list;
  OMP_DIRECTIVE(parallel) {
    OMP_DIRECTIVE(single)
    for (int i = 0; i < omp::get_num_threads(); ++i) {
      // Note: each generator generates roughly rmat_option.edge_count /
      // num_threads
      generator_list.push_back(std::make_unique<rmat_generator>(
          rmat_option.seed + i, rmat_option.vertex_scale,
          rmat_option.edge_count, rmat_option.a, rmat_option.b, rmat_option.c,
          rmat_option.scramble_id, false));
      generator_itr_list.push_back(generator_list.at(i)->begin());
    }
  }

  auto input_storage = allocate_key_value_input_storage<adjacency_list_type>();
  const std::size_t total_edges =
      rmat_option.edge_count * (rmat_option.undirected ? 2 : 1);
  std::size_t count_loop = 0;
  double total_elapsed_time = 0;
  while (true) {
    const ssize_t num_generate = std::min(
        (ssize_t)chunk_size, (ssize_t)(total_edges - count_loop * chunk_size));
    if (num_generate <= 0) break;
    if (verbose) std::cout << "\n[ " << count_loop << " ]" << std::endl;

    // -- Generate rmat edges -- //
    OMP_DIRECTIVE(parallel) {
      assert((int)input_storage.size() == (int)omp::get_num_threads());
      auto &local_list = input_storage.at(omp::get_thread_num());
      local_list.clear();
      const auto range = metall::mtlldetail::partial_range(
          num_generate, omp::get_thread_num(), omp::get_num_threads());
      auto &itr = generator_itr_list.at(omp::get_thread_num());
      for (std::size_t i = range.first; i < range.second; ++i) {
        local_list.emplace_back(*itr);
        ++itr;
      }
    }

    total_elapsed_time += ingest_key_values(input_storage, preprocess,
                                            postprocess, adj_list, verbose);

    if (ofs_save_edge) {
      for (const auto &list : input_storage) {
        for (const auto &elem : list) {
          *ofs_save_edge << elem.first << "\t" << elem.second << "\n";
        }
      }
    }

    ++count_loop;
  }
  ofs_save_edge->close();

  return total_elapsed_time;
}

template <typename adjacency_list_type>
inline void dump_adj_list(const adjacency_list_type &adj_list,
                          const std::string &file_name) {
  std::cout << "Dumping adjacency list..." << std::endl;

  std::ofstream ofs(file_name);
  if (!ofs.is_open()) {
    std::cerr << "Cannot open " << file_name << std::endl;
    return;
  }

  for (auto key_itr = adj_list.keys_begin(), key_end = adj_list.keys_end();
       key_itr != key_end; ++key_itr) {
    for (auto value_itr = adj_list.values_begin(key_itr->first),
              value_end = adj_list.values_end(key_itr->first);
         value_itr != value_end; ++value_itr) {
      ofs << key_itr->first << " " << *value_itr << "\n";
    }
  }
  ofs.close();

  if (!ofs) {
    std::cerr << "Failed to write data to " << file_name << std::endl;
    std::abort();
  }

  std::cout << "Finished" << std::endl;
};

/// \brief Run Key-value insertion benchmark.
/// \tparam adjacency_list_type A type of the adjacent-list type to use.
/// \param options Benchmark options.
/// \param preprocess A function that is called after each chunk is inserted.
/// if '!preprocess' is true, it won't be called.
/// \param adj_list A pointer to a adjacently list object.
template <typename adjacency_list_type>
void run_bench(
    const bench_options &options, adjacency_list_type *adj_list,
    const std::function<void()> &preprocess = std::function<void()>{},
    const std::function<void()> &postprocess = std::function<void()>{}) {
  std::cout << "Start key-value data ingestion" << std::endl;
  print_omp_configuration();

  std::ofstream ofs_save_edge;
  if (!options.edge_list_dump_file_name.empty()) {
    std::cout << "Dump edge list during the benchmark: "
              << options.edge_list_dump_file_name << std::endl;
    ofs_save_edge.open(options.edge_list_dump_file_name);
    if (!ofs_save_edge.is_open()) {
      std::cerr << "Cannot open " << options.edge_list_dump_file_name
                << std::endl;
      std::abort();
    }
  }

  double elapsed_time_sec;
  if (options.input_file_name_list.empty()) {
    std::cout << "Get inputs from an R-MAT edge generator (graph data)"
              << std::endl;
    elapsed_time_sec = run_bench_rmat_edge(options.rmat, options.chunk_size,
                                           preprocess, postprocess, adj_list,
                                           &ofs_save_edge, options.verbose);
  } else {
    std::cout << "Get inputs from key-value files" << std::endl;
    elapsed_time_sec = run_bench_kv_file(
        options.input_file_name_list, options.chunk_size, preprocess,
        postprocess, adj_list, &ofs_save_edge, options.verbose);
  }
  std::cout << "\nIngesting all data took (s)\t" << elapsed_time_sec
            << std::endl;

  if (ofs_save_edge.is_open()) {
    ofs_save_edge.close();
    if (!ofs_save_edge) {
      std::cerr << "Failed to write edges to "
                << options.edge_list_dump_file_name << std::endl;
      std::abort();
    }
  }

  if (!options.adj_list_dump_file_name.empty()) {
    dump_adj_list(*adj_list, options.adj_list_dump_file_name);
  }
}

}  // namespace adjacency_list_bench
#endif  // METALL_BENCH_ADJACENCY_LIST_BENCH_DRIVER_HPP
