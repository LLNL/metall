// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_MEMORY_HPP
#define METALL_DETAIL_UTILITY_MEMORY_HPP

#include <unistd.h>
#include <iostream>
#include <cstdio>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

inline ssize_t get_page_size() noexcept {
  const ssize_t page_size = ::sysconf(_SC_PAGE_SIZE);
  if (page_size == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__,
                   "Failed to get the page size");
  }

  return page_size;
}

/// \brief Reads a value from /proc/meminfo
/// \param key Target token looking for
/// \return On success, returns read value. On error, returns -1.
inline ssize_t read_meminfo(const std::string &key) {
  std::ifstream fin("/proc/meminfo");
  if (!fin.is_open()) {
    return -1;
  }

  std::string key_with_colon(key);
  if (key_with_colon.at(key_with_colon.length() - 1) != ':') {
    key_with_colon.append(":");
  }
  std::string token;
  while (fin >> token) {
    if (token != key_with_colon) continue;

    std::size_t value;
    if (!(fin >> value)) {
      return -1;
    }

    std::string unit;
    if (fin >> unit) {
      std::transform(unit.begin(), unit.end(), unit.begin(),
                     [](const unsigned char c) { return std::tolower(c); });
      if (unit == "kb") {  // for now, we only expect this case
        return value * 1024;
      }
      return -1;
    } else {  // found a line does not has unit
      return value;
    }
  }
  return -1;
}

/// \brief Returns the size of the total ram size
/// \return On success, returns the total ram size of the system. On error,
/// returns -1.
inline ssize_t get_total_ram_size() {
  const ssize_t mem_total = read_meminfo("MemTotal:");
  if (mem_total == -1) {
    return -1;  // something wrong;
  }
  return static_cast<ssize_t>(mem_total);
}

/// \brief Returns the size of used ram size from /proc/meminfo
/// \return On success, returns the used ram size. On error, returns -1.
inline ssize_t get_used_ram_size() {
  const ssize_t mem_total = read_meminfo("MemTotal:");
  const ssize_t mem_free = read_meminfo("MemFree:");
  const ssize_t buffers = read_meminfo("Buffers:");
  const ssize_t cached = read_meminfo("Cached:");
  const ssize_t slab = read_meminfo("Slab:");
  const ssize_t used = mem_total - mem_free - buffers - cached - slab;
  if (mem_total == -1 || mem_free == -1 || buffers == -1 || cached == -1 ||
      slab == -1 || used < 0) {
    return -1;  // something wrong;
  }
  return used;
}

/// \brief Returns the size of free ram size from /proc/meminfo
/// \return On success, returns the free ram size. On error, returns -1.
inline ssize_t get_free_ram_size() {
  const ssize_t mem_free = read_meminfo("MemFree:");
  if (mem_free == -1) {
    return -1;  // something wrong;
  }
  return static_cast<ssize_t>(mem_free);
}

/// \brief Returns the size of the 'cached' ram size
/// \return On success, returns the 'cached' ram size of the system. On error,
/// returns -1.
inline ssize_t get_page_cache_size() {
  const ssize_t cached_size = read_meminfo("Cached:");
  if (cached_size == -1) {
    return -1;  // something wrong;
  }
  return static_cast<ssize_t>(cached_size);
}

/// \brief Returns the number of page faults caused by the process
/// \return A pair of #of minor and major page faults
inline std::pair<std::size_t, std::size_t> get_num_page_faults() {
  std::size_t minflt = 0;
  std::size_t majflt = 0;
#ifdef __linux__
  const char *stat_path = "/proc/self/stat";
  FILE *f = ::fopen(stat_path, "r");
  if (f) {
    // 0:pid 1:comm 2:state 3:ppid 4:pgrp 5:session 6:tty_nr 7:tpgid 8:flags
    // 9:minflt 10:cminflt 11:majflt
    int ret;
    if ((ret = ::fscanf(f, "%*d %*s %*c %*d %*d %*d %*d %*d %*u %lu %*u %lu",
                        &minflt, &majflt)) != 2) {
      std::stringstream ss;
      ss << "Failed to reading #of page faults " << ret;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      minflt = majflt = 0;
    }

    fclose(f);
  }
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "get_num_page_faults() is not supported in this environment"
#endif
#endif
  return std::make_pair(minflt, majflt);
}

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_MEMORY_HPP
