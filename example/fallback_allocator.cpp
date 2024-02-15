// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>

#include <metall/metall.hpp>
#include <metall/container/vector.hpp>

using vector_t =
    metall::container::vector<int, metall::manager::fallback_allocator<int>>;

int main() {
  // Allocation using Metall
  {
    metall::manager manager(metall::create_only, "/tmp/dir");
    auto pvec = manager.construct<vector_t>("vec")(manager.get_allocator());
    pvec->push_back(1);
    std::cout << (*pvec)[0] << std::endl;
  }

  // Allocation not using Metall, i.e., uses a heap allocator (malloc()).
  // This code would cause a build error if fallback_allocator were not used.
  {
    vector_t vec;  // As no metall allocator argument is passed, the fallback
                   // allocator uses malloc() internally.
    vec.push_back(2);
    std::cout << vec[0] << std::endl;
  }

  return 0;
}