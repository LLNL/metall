// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to make and use an allocator-aware data structure.

#include <iostream>
#include <metall/metall.hpp>
#include "t4-1.hpp"

using persit_array = dynamic_array<int, metall::manager::allocator_type<int>>;

int main() {
  // Creating data into persistent memory
  {
    metall::manager manager(metall::create_only, "/tmp/dir");
    auto* array =
        manager.construct<persit_array>("array")(manager.get_allocator());
    init(*array);
  }

  // Reattaching the data
  {
    metall::manager manager(metall::open_only, "/tmp/dir");
    auto* array = manager.find<persit_array>("array").first;
    print(*array);
    manager.destroy_ptr(array);
  }

  // This is how to use dynamic_array using the normal allocator
  // As you can see, the same data structure works with Metall and the normal
  // allocator.
  {
    auto* array = new dynamic_array<int>();
    init(*array);
    print(*array);
    delete array;
  }

  return 0;
}