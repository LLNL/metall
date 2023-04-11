// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_BFS_KERNEL_HPP
#define METALL_BENCH_BFS_KERNEL_HPP

#include <iostream>
#include <limits>
#include <vector>
#include <numeric>

#include <metall/utility/open_mp.hpp>

namespace bfs_bench {

/// \brief Data structures used in the BFS kernel
struct bfs_data {
  using level_type = uint16_t;
  static constexpr level_type k_infinite_level =
      std::numeric_limits<level_type>::max();

  void initialize(const std::size_t max_vertex_id) {
    level.resize(max_vertex_id + 1);
    visited_filter.resize(max_vertex_id + 1);
    reset();
  }

  void reset() {
    std::fill(level.begin(), level.end(), k_infinite_level);
    std::fill(visited_filter.begin(), visited_filter.end(), 0);
  }

  std::vector<level_type> level;
  std::vector<bool> visited_filter;
};

/// \brief Allocate and initialize bfs data
template <typename vertex_id_type>
void initialize(const std::size_t max_vertex_id, const vertex_id_type &source,
                bfs_data *data) {
  data->initialize(max_vertex_id);
  data->level[source] = 0;
  data->visited_filter[source] = true;
}

/// \brief Run BFS
/// \return
template <typename graph_type>
bfs_data::level_type kernel(const graph_type &graph, bfs_data *data) {
  bfs_data::level_type current_level = 0;
  bool visited_new_vertex = false;
  const std::size_t num_vertices = data->level.size();
  while (true) {  /// BFS main loop

    /// BFS loop for a single level
    /// We assume that the cost of generating threads at every level is
    /// negligible
    OMP_DIRECTIVE(parallel for schedule (runtime))
    for (typename graph_type::key_type source = 0; source < num_vertices;
         ++source) {
      if (graph.num_values(source) == 0) continue;
      if (data->level.at(source) != current_level) continue;

      for (auto neighbor_itr = graph.values_begin(source),
                neighbor_end = graph.values_end(source);
           neighbor_itr != neighbor_end; ++neighbor_itr) {
        const auto &neighbor = *neighbor_itr;
        if (!data->visited_filter.at(neighbor) &&
            data->level.at(neighbor) == bfs_data::k_infinite_level) {
          data->level.at(neighbor) = current_level + 1;
          data->visited_filter.at(neighbor) = true;
          visited_new_vertex = true;
        }
      }
    }

    if (!visited_new_vertex) break;

    ++current_level;
    visited_new_vertex = false;
  }  /// End of BFS main loop

  return current_level;
}

void count_level(const bfs_data &data) {
  bfs_data::level_type max_level = 0;
  for (auto level : data.level) {
    if (level == bfs_data::k_infinite_level) continue;
    max_level = std::max(level, max_level);
  }

  std::vector<std::size_t> cnt(max_level + 1, 0);
  for (auto level : data.level) {
    if (level == bfs_data::k_infinite_level) continue;
    ++cnt[level];
  }

  std::cout << "Level\t#vertices" << std::endl;
  for (uint16_t i = 0; i <= max_level; ++i) {
    std::cout << i << "\t" << cnt[i] << std::endl;
  }
  std::cout << "Total\t"
            << std::accumulate(cnt.begin(), cnt.end(),
                               static_cast<std::size_t>(0))
            << std::endl;
}

}  // namespace bfs_bench
#endif  // METALL_BENCH_BFS_KERNEL_HPP
