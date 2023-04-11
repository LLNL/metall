// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_NUMA_HPP
#define METALL_BENCH_UTILITY_NUMA_HPP

#ifdef METALL_USE_NUMA_LIB
#include <numa.h>
#else
#warning "Does not use NUMA"
#endif

#include <metall/detail/utilities.hpp>

namespace bench_utility::numa {

bool available() noexcept {
#ifdef METALL_USE_NUMA_LIB
  return ::numa_available();
#else
  return false;
#endif
}

int get_avail_nodes() noexcept {
#ifdef METALL_USE_NUMA_LIB
  return ::numa_num_task_nodes();
#else
  return 1;
#endif
}

int get_node([[maybe_unused]] const int thread_id) noexcept {
#ifdef METALL_USE_NUMA_LIB
  return thread_id % get_avail_nodes();
#else
  return 0;
#endif
}

int set_node([[maybe_unused]] const int thread_id) noexcept {
#ifdef METALL_USE_NUMA_LIB
  struct bitmask *mask = numa_bitmask_alloc(numa_num_possible_nodes());
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

int get_local_num_threads([[maybe_unused]] const int thread_id,
                          const int num_threads) noexcept {
#ifdef METALL_USE_NUMA_LIB
  const auto range = metall::mtlldetail::partial_range(
      num_threads, get_node(thread_id), get_avail_nodes());
  return range.second - range.first;
#else
  return num_threads;
#endif
}

int get_local_thread_num(const int thread_id) noexcept {
#ifdef METALL_USE_NUMA_LIB
  return thread_id / get_avail_nodes();
#else
  return thread_id;
#endif
}

void *alloc_local(const std::size_t size) noexcept {
#ifdef METALL_USE_NUMA_LIB
  return ::numa_alloc_local(size);
#else
  return ::malloc(size);
#endif
}

void free(void *start, [[maybe_unused]] const std::size_t size) noexcept {
#ifdef METALL_USE_NUMA_LIB
  ::numa_free(start, size);
#else
  ::free(start);
#endif
}
}  // namespace bench_utility::numa
#endif  // METALL_BENCH_UTILITY_NUMA_HPP
