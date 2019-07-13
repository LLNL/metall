// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_NUMA_HPP
#define METALL_BENCH_UTILITY_NUMA_HPP

#ifdef __linux__
#include <numa.h>
#endif

#include <metall/detail/utility/common.hpp>

namespace numa {
bool available() noexcept {
#ifdef __linux__
  return ::numa_available();
#else
  return false;
#endif
}

int get_avail_nodes() noexcept {
#ifdef __linux__
  return ::numa_num_task_nodes();
#else
  return 1;
#endif
}

int get_node(const int thread_id) noexcept {
#ifdef __linux__
  return thread_id % get_avail_nodes();
#else
  return 0;
#endif
}

int set_node(const int thread_id) noexcept {
#ifdef __linux__
  struct bitmask * mask = numa_bitmask_alloc(numa_num_possible_nodes());
  assert(mask);
  const int node = get_node(thread_id);
  numa_bitmask_setbit(mask, node);
  numa_bind(mask);
  numa_bitmask_free(mask);
  return node;
#else
  return 0;
#endif
}

int get_local_num_threads(const int thread_id, const int num_threads) noexcept {
#ifdef __linux__
  const auto range = utility::partial_range(num_threads, get_node(thread_id), get_avail_nodes());
  return range.second - range.first;
#else
  return num_threads;
#endif
}

int get_local_thread_num(const int thread_id) noexcept {
#ifdef __linux__
  return thread_id / get_avail_nodes();
#else
  return thread_id;
#endif
}

void *alloc_local(const std::size_t size) noexcept {
#ifdef __linux__
  return ::numa_alloc_local(size);
#else
  return ::malloc(size);
#endif
}

void free(void *start, const std::size_t size) noexcept {
#ifdef __linux__
  ::numa_free(start, size);
#else
  ::free(start);
#endif
}
}
#endif //METALL_BENCH_UTILITY_NUMA_HPP
