// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <vector>
#include <string>

#include <metall/manager.hpp>
#include "kernel.hpp"

template <typename T>
using allocator_type = typename metall::manager::allocator_type<T>;


int main(int, char *argv[])
{
  const std::size_t min_alloc_size = std::stoll(argv[1]);
  const std::size_t max_alloc_size = std::stoll(argv[2]);
  const uint64_t num_alloc = std::stoll(argv[3]);
  const std::string segment_name = argv[4];

  for (std::size_t alloc_size = min_alloc_size; alloc_size <= max_alloc_size; alloc_size *= 2) {
    metall::manager manager(metall::create_only, segment_name.c_str(), max_alloc_size * num_alloc * 2);
    bench_basic::kernel(alloc_size, num_alloc, manager.get_allocator<void>());
  }

  return 0;
}