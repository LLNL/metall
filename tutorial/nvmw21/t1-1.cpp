// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp> // Only one header file is required to be included

int main() {

  {
    metall::manager manager(metall::create_only, "/tmp/dir");

    int *n = manager.construct<int> // Allocates an 'int' object
                    ("name") // Stores the allocated memory address with key "name"
                    (10); // Call a constructor of the object
  }

  {
    metall::manager manager(metall::open_only, "/tmp/dir");

    int *n = manager.find<int>("name").first;
    std::cout << *n << std::endl;

    manager.destroy_ptr(n); // Deallocate memory
  }

  return 0;
}