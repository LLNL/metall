// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cstddef>

#include <metall/metall.hpp>
#include "kernel.hpp"

int main(int argc, char *argv[]) {
  const auto option = simple_alloc_bench::parse_option(argc, argv);
  {
    metall::manager manager(metall::create_only, option.datastore_path);
    simple_alloc_bench::run_bench(option, manager.get_allocator<std::byte>());
  }
  metall::manager::remove(option.datastore_path.c_str());

  return 0;
}