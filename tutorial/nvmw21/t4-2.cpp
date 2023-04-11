// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to make and use an allocator-aware data structure.

#include <iostream>
#include <metall/metall.hpp>
#include "t4-2.hpp"

using persit_matrix = matrix<float, metall::manager::allocator_type<float>>;

int main() {
  // Creating data into persistent memory
  {
    metall::manager manager(metall::create_only, "/tmp/dir");
    auto* mx = manager.construct<persit_matrix>("mx")(manager.get_allocator());
    init_matrix(*mx);
  }

  // Reattaching the data
  {
    metall::manager manager(metall::open_only, "/tmp/dir");
    auto* mx = manager.find<persit_matrix>("mx").first;
    print_matrix(*mx);
    manager.destroy_ptr(mx);
  }

  // This is how to use matrix using the normal allocator
  // As you can see, the same data structure works with Metall and the normal
  // allocator.
  {
    auto* mx = new matrix<float>();
    init_matrix(*mx);
    print_matrix(*mx);
    delete mx;
  }

  return 0;
}
