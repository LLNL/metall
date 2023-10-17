// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PROC_HPP
#define METALL_DETAIL_UTILITY_PROC_HPP

#include <sched.h>

#ifdef _GNU_SOURCE
#include <sys/sysinfo.h>
#define SUPPORT_GET_CPU_CORE_NO true
#else
#define SUPPORT_GET_CPU_CORE_NO false
#endif

#include <thread>

namespace metall::mtlldetail {

/// \brief Returns the number of the logical CPU core on which the calling
/// thread is currently executing.
inline int get_cpu_core_no() {
#if SUPPORT_GET_CPU_CORE_NO

  const int cpu = ::sched_getcpu();
  if (cpu == -1) {
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

/// \brief Returns the number of the logical CPU cores on the system.
inline int get_num_cpu_cores() {
#if SUPPORT_GET_CPU_CORE_NO
  return get_nprocs_conf();
#else
  return int(std::thread::hardware_concurrency());
#endif
}

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_PROC_HPP
