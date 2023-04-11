// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This is an example shows how to store pointers in persistent memory using the
// offset pointer

#include <iostream>
#include <metall/metall.hpp>

// metall::offset_ptr is just an alias of offset_ptr in Boost.Interprocess
// https://www.boost.org/doc/libs/release/doc/html/interprocess/offset_ptr.html
using int_offset_prt = metall::offset_ptr<int>;

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");

    // Allocate a simple array in persistent memory
    int *array = static_cast<int *>(manager.allocate(10 * sizeof(int)));
    array[0] = 1;
    array[1] = 2;

    // Allocate an offset pointer with key 'ptr' and initialize its value with
    // the address of 'array'
    [[maybe_unused]] int_offset_prt *ptr =
        manager.construct<int_offset_prt>("ptr")(array);
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");

    int_offset_prt *ptr = manager.find<int_offset_prt>("ptr").first;

    // metall::to_raw_pointer extracts a raw pointer from metall::offset_ptr
    // If a raw pointer is given, it just returns the address the given pointer
    // points to
    int *array = metall::to_raw_pointer(*ptr);

    std::cout << array[0] << std::endl;  // Print 1
    std::cout << array[1] << std::endl;  // Print 2

    // Deallocate the array
    manager.deallocate(metall::to_raw_pointer(*ptr));
    *ptr = nullptr;

    // Destroy the offset pointer object
    manager.destroy<int_offset_prt>("ptr");
  }

  return 0;
}