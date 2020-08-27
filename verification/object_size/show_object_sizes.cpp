// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp>
#include <metall/kernel/object_size_manager.hpp>

static constexpr std::size_t k_max_segment_size = 1ULL << 48ULL;
using object_size_manager = metall::kernel::object_size_manager<metall::manager::chunk_size(), k_max_segment_size>;

int main() {
  std::cout << "Bin number\t:\tSize" << std::endl;
  for (uint64_t i = 0; i < object_size_manager::num_sizes(); ++i) {
    const auto size = object_size_manager::at(i);
    std::cout << i << "\t:\t" << size << std::endl;
  }

  return 0;
}