// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp> // Only one header file is required to be included

struct my_data {
  my_data(int _n, double _f)
      : n(_n), f(_f) {}

  int n;
  double f;
};

int main() {

  {
    metall::manager manager(metall::create_only, "/tmp/dir");

    manager.construct<my_data>      // Allocates an 'my_data' object
                    ("data") // Stores the allocated memory address with key "name"
                    (10, 20.0);    // Call a constructor of the object
  }

  {
    metall::manager manager(metall::open_read_only, "/tmp/dir");

    auto *data = manager.find<my_data>("data").first;
    std::cout << data->n << " " << data->f << std::endl;

    manager.destroy_ptr(data);// Deallocate memory
  }

  return 0;
}