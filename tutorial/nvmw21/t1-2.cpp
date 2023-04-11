// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp>

// This program allocates a user-defined struct/class object and reattaches it
// using Metall. One can learn how non-primitive data type object is constructed
// and destructed with Metall.

struct my_data {
  my_data(int _n, double _f) : n(_n), f(_f) {
    std::cout << "Constructor is called" << std::endl;
  }

  ~my_data() { std::cout << "Destructor is called" << std::endl; }

  int n;
  double f;
};

int main() {
  // Creating data into persistent memory
  {
    metall::manager manager(metall::create_only, "/tmp/dir");

    std::cout << "Allocate and construct an object" << std::endl;
    manager.construct<my_data>  // Allocates an 'my_data' object
        ("data")     // Stores the allocated memory address with key "name"
        (10, 20.0);  // Call the constructor of the class
  }

  // ----------------------------------------------------------------------------------------------------
  // Assume that this program exits here and the following code block is
  // executed as another run
  // ----------------------------------------------------------------------------------------------------

  // Reattaching the data
  {
    metall::manager manager(metall::open_read_only, "/tmp/dir");

    auto *data = manager.find<my_data>("data").first;
    std::cout << data->n << " " << data->f << std::endl;

    manager.destroy_ptr(data);  // Destruct the object and deallocate memory
  }

  return 0;
}