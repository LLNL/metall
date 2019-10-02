// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This is an example shows how to store pointers in persistent memory using the offset pointer

#include <iostream>
#include <metall/metall.hpp>

struct my_class {
  metall::offset_ptr<int> array; // Offset pointer to hold int*
};

int main()
{
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");

    int* array = static_cast<int *>(manager.allocate(10 * sizeof(int))); // Allocate an array in persistent memory
    array[0] = 1;
    array[1] = 2;

    auto object = manager.construct<my_class>("root_object")();
    object->array = array; // Store the address (offset) in an object allocated in persistent memory
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");

    auto object = manager.find<my_class>("root_object").first;

    std::cout << object->array[0] << std::endl; // Will print 1
    std::cout << object->array[1] << std::endl; // Will print 2

    // Deallocate the array
    // get() method returns a raw address
    manager.deallocate(object->array.get());

    manager.destroy<my_class>("roo_object");
  }

  return 0;
}