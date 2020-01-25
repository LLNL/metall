// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PROC_HPP
#define METALL_DETAIL_UTILITY_PROC_HPP

#if defined(__linux__)
#include <linux/getcpu.h>
#endif

namespace metall {
namespace detail {
namespace utility {

/// \brief Returns the number of the CPU core on which the calling thread is currently executing
/// \return Returns a nonnegative CPU core number
inline int get_cpu_core_no() {
#if defined(__linux__)
  int cpu;
  if (::getcpu(&cpu, nullptr, nullptr) == -1) {
    return 0;
  }
  return cpu;
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "CPU core number is always 0"
#endif
  return 0;
#endif
}

/// \brief Returns the number of the NUMA node on which the calling thread is currently executing
/// \return Returns a nonnegative NUMA node number
inline int get_numa_node_num() {
#if defined(__linux__)
  int numa_node;
  if (::getcpu(&nullptr, &numa_node, nullptr) == -1) {
    return 0;
  }
  return numa_node;
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "NUMA node number is always 0"
#endif
  return 0;
#endif
}

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY_PROC_HPP
