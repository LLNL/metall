// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_OPEN_MP_HPP
#define METALL_BENCH_UTILITY_OPEN_MP_HPP

#include <string>

#ifdef _OPENMP
#include <omp.h>
#endif

namespace omp {

using omp_sched_type =
#ifdef _OPENMP
omp_sched_t;
#else
int;
#endif

inline std::string schedule_kind_name([[maybe_unused]] const omp_sched_type kind) {
  std::string name;

#ifdef _OPENMP
  #if _OPENMP >= 201511
    if (kind == omp_sched_static || kind == (omp_sched_static | omp_sched_monotonic)) {
      name = "omp_sched_static";
    } else if (kind == omp_sched_dynamic || kind == (omp_sched_dynamic | omp_sched_monotonic)) {
      name = "omp_sched_dynamic";
    } else if (kind == omp_sched_guided || kind == (omp_sched_guided | omp_sched_monotonic)) {
      name = "omp_sched_guided";
    } else if (kind == omp_sched_auto || kind == (omp_sched_auto | omp_sched_monotonic)) {
      name = "omp_sched_auto";
    } else {
      name = "Unknown kind (" + std::to_string((uint64_t)kind) + ")";
    }
    if (kind & omp_sched_monotonic) {
      name += " with omp_sched_monotonic";
    }
  #else
    if (kind == omp_sched_static) {
      name = "omp_sched_static";
    } else if (kind == omp_sched_dynamic) {
      name = "omp_sched_dynamic";
    } else if (kind == omp_sched_guided) {
      name = "omp_sched_guided";
    } else if (kind == omp_sched_auto) {
      name = "omp_sched_auto";
    } else {
      name = "Unknown kind (" + std::to_string((uint64_t)kind) + ")";
    }
  #endif
#else
  name = "OpenMP is not supported";
#endif

  return name;
}

inline std::pair<omp_sched_type, int> get_schedule() {
#ifdef _OPENMP
  omp_sched_t kind;
  int chunk_size;
  ::omp_get_schedule(&kind, &chunk_size);

  return std::make_pair(kind, chunk_size);
#else
  return std::make_pair(0, 0);
#endif
}

inline int get_num_threads() noexcept {
#ifdef _OPENMP
  return ::omp_get_num_threads();
#else
  return 1;
#endif
}

inline int get_thread_num() noexcept {
#ifdef _OPENMP
  return ::omp_get_thread_num();
#else
  return 0;
#endif
}

} // namespace utility

#endif //METALL_BENCH_UTILITY_OPEN_MP_HPP
