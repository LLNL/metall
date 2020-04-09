// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/kernel/object_size_manager.hpp>

int main() {

  static constexpr std::size_t chunk_size = 1UL << 20UL;
  static constexpr std::size_t k_max_segment_size = 1ULL << 48ULL;
  using object_size_manager = metall::kernel::object_size_manager<chunk_size, k_max_segment_size>;

  std::cout << "Bin number\t:\tSize" << std::endl;
  for (std::size_t i = 0; i < object_size_manager::num_sizes(); ++i) {
    std::cout << i << "\t:\t" << object_size_manager::at(i) << std::endl;
  }

  return 0;
}