// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <memory>
#include <cstddef>

#include "kernel.hpp"

int main(int argc, char *argv[]) {
  const auto option = simple_alloc_bench::parse_option(argc, argv);

  simple_alloc_bench::run_bench(option, std::allocator<std::byte>());

  return 0;
}