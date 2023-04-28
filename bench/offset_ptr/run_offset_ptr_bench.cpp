// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks the offset_ptr.
/// Usage:
/// ./run_offset_ptr_bench

#include <iostream>
#include <metall/offset_ptr.hpp>
#include <metall/detail/time.hpp>

namespace mdtl = metall::mtlldetail;

int main() {
  const std::size_t length = 1ULL << 22;
  auto *array = new uint64_t[length];

  for (std::size_t i = 0; i < length; ++i) {
    array[i] = i;
  }
  std::cout << "Initialized array, length = " << length << std::endl;

  {
    auto *ptr = array;
    const auto start = mdtl::elapsed_time_sec();
    for (std::size_t i = 0; i < length; ++i) {
      [[maybe_unused]] volatile const uint64_t x = *ptr;
      ++ptr;
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Raw pointer took (s)\t" << elapsed_time << std::endl;
  }

  {
    auto ofset_ptr = metall::offset_ptr<uint64_t>(array);
    const auto start = mdtl::elapsed_time_sec();
    for (std::size_t i = 0; i < length; ++i) {
      [[maybe_unused]] volatile const uint64_t x = *ofset_ptr;
      ++ofset_ptr;
    }
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Offset pointer took (s)\t" << elapsed_time << std::endl;
  }

  return 0;
}