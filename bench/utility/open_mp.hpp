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

inline std::string schedule_kind_name(const int kind_in_int) {
#ifdef _OPENMP
  if (kind_in_int == omp_sched_static) {
    return std::string("omp_sched_static (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_dynamic) {
    return std::string("omp_sched_dynamic (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_guided) {
    return std::string("omp_sched_guided (" + std::to_string(kind_in_int) + ")");
  } else if (kind_in_int == omp_sched_auto) {
    return std::string("omp_sched_auto (" + std::to_string(kind_in_int) + ")");
  }
  return std::string("Unknown kind (" + std::to_string(kind_in_int) + ")");
#else
  return std::string("OpenMP is not supported");
#endif
};

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
