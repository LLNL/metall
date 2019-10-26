// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <memory>

#include "kernel.hpp"

int main(int, char *argv[]) {

  const std::size_t min_alloc_size = std::stoll(argv[1]);
  const std::size_t max_alloc_size = std::stoll(argv[2]);
  const uint64_t num_alloc = std::stoll(argv[3]);

  for (std::size_t alloc_size = min_alloc_size; alloc_size <= max_alloc_size; alloc_size *= 2) {
    bench_basic::kernel(alloc_size, num_alloc, std::allocator<std::byte>());
  }

  return 0;
}