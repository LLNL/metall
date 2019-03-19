// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_BASIC_KERNEL_HPP
#define METALL_BENCH_BASIC_KERNEL_HPP

#include <iostream>
#include <memory>
#include <vector>

#include "../utility/time.hpp"

namespace bench_basic {

template<typename allocator_type>
void kernel(const std::size_t alloc_size, const std::size_t num_allocations, allocator_type allocator) {

  using char_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<char>;
  char_allocator_type char_allocator(allocator);
  std::vector<typename char_allocator_type::pointer> addr_list(num_allocations);

  std::cout << "Allocation size: " << alloc_size << std::endl;
  std::cout << "#of allocations: " << num_allocations << std::endl;
  std::cout << "Total allocation size will be: " << alloc_size * num_allocations << std::endl;

  {
    const auto start = utility::elapsed_time_sec();
    for (std::size_t i = 0; i < num_allocations; ++i) {
      addr_list[i] = char_allocator.allocate(alloc_size);
    }
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "Allocation took:\t" << elapsed_time << std::endl;
  }

  {
    const auto start = utility::elapsed_time_sec();
    for (std::size_t i = 0; i < num_allocations; ++i) {
      char_allocator.deallocate(addr_list[i], alloc_size);
    }
    const auto elapsed_time = utility::elapsed_time_sec(start);
    std::cout << "Deallocation took:\t" << elapsed_time << std::endl;
  }
  std::cout << std::endl;
}
}

#endif //METALL_BENCH_BASIC_KERNEL_HPP
