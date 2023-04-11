// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_TIME_HPP
#define METALL_DETAIL_UTILITY_TIME_HPP

#include <chrono>

namespace metall::mtlldetail {

inline std::chrono::high_resolution_clock::time_point elapsed_time_sec() {
  return std::chrono::high_resolution_clock::now();
}

inline double elapsed_time_sec(
    const std::chrono::high_resolution_clock::time_point &tic) {
  auto duration_time = std::chrono::high_resolution_clock::now() - tic;
  return static_cast<double>(
             std::chrono::duration_cast<std::chrono::microseconds>(
                 duration_time)
                 .count()) /
         1e6;
}

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_TIME_HPP
