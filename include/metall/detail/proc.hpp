// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PROC_HPP
#define METALL_DETAIL_UTILITY_PROC_HPP

#include <sched.h>

#ifdef _GNU_SOURCE
#include <sys/sysinfo.h>
#define SUPPORT_GET_CPU_NO true
#else
#define SUPPORT_GET_CPU_NO false
#endif

#include <thread>

namespace metall::mtlldetail {

/// \brief Returns the number of the logical CPU core on which the calling
/// thread is currently executing.
inline unsigned int get_cpu_no() {
#if SUPPORT_GET_CPU_NO

  const int cpu = ::sched_getcpu();
  if (cpu == -1) {
    return 0;
  }
  return cpu;

#else

#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "CPU number is always 0"
#endif
  return 0;

#endif
}

/// \brief Returns the number of the logical CPU cores on the system.
inline unsigned int get_num_cpus() {
#if SUPPORT_GET_CPU_NO
  return ::get_nprocs_conf();
#else
  return std::thread::hardware_concurrency();
#endif
}

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_PROC_HPP
