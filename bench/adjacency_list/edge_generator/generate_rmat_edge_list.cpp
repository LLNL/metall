// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <fstream>
#include <string>
#include <vector>

#include "rmat_edge_generator.hpp"

// ---------------------------------------- //
// Option
// ---------------------------------------- //
struct rmat_option_t {
  uint32_t seed{123};
  uint64_t vertex_scale{17};
  uint64_t edge_count{(1ULL << 17) * 16};
  double a{0.57};
  double b{0.19};
  double c{0.19};
  bool scramble_id{true};
  bool undirected{true};
};

bool parse_options(int argc, char **argv, rmat_option_t *option, std::string *edge_list_file_name) {
  int p;
  while ((p = getopt(argc, argv, "o:s:v:e:a:b:c:r:u:")) != -1) {
    switch (p) {
      case 'o':*edge_list_file_name = optarg;
        break;

      case 's':option->seed = static_cast<uint32_t>(std::stoull(optarg));
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

      default:std::cerr << "Invalid option" << std::endl;
        return false;
    }
  }

  std::cout << "seed: " << option->seed
            << "\nvertex_scale: " << option->vertex_scale
            << "\nedge_count: " << option->edge_count
            << "\na: " << option->a
            << "\nb: " << option->b
            << "\nc: " << option->c
            << "\nscramble_id: " << static_cast<int>(option->scramble_id)
            << "\nundirected: " << static_cast<int>(option->undirected)
            << "\nedge_list_file_name: " << *edge_list_file_name << std::endl;

  return true;
}

int main(int argc, char **argv) {

  rmat_option_t rmat_option;
  std::string edge_list_file_name;
  parse_options(argc, argv, &rmat_option, &edge_list_file_name);

  edge_generator::rmat_edge_generator rmat(rmat_option.seed, rmat_option.vertex_scale, rmat_option.edge_count,
                                           rmat_option.a, rmat_option.b, rmat_option.c,
                                           rmat_option.scramble_id, rmat_option.undirected);

  std::ofstream edge_list_file(edge_list_file_name);
  if (!edge_list_file.is_open()) {
    std::cerr << "Cannot open " << edge_list_file_name << std::endl;
    std::abort();
  }

  for (auto edge : rmat) {
    // std::cout << edge.first << " " << edge.second << "\n"; // DB
    edge_list_file << edge.first << " " << edge.second << "\n";
  }
  edge_list_file.close();

  std::cout << "Generation done" << std::endl;

  return 0;
}