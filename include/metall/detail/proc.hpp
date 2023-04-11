// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PROC_HPP
#define METALL_DETAIL_UTILITY_PROC_HPP

#include <sched.h>

namespace metall::mtlldetail {
#ifdef _GNU_SOURCE
#define SUPPORT_GET_CPU_CORE_NO true
#else
#define SUPPORT_GET_CPU_CORE_NO false
#endif

/// \brief Returns the number of the CPU core on which the calling thread is
/// currently executing \return Returns a non-negative CPU core number
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

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_PROC_HPP
