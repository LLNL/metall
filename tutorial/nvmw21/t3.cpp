// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to store pointers in persistent memory using the
// offset pointer

#include <iostream>
#include <metall/metall.hpp>

// metall::offset_ptr is just an alias of offset_ptr in Boost.Interprocess
// https://www.boost.org/doc/libs/release/doc/html/interprocess/offset_ptr.html

struct my_data {
  int len{0};
  metall::offset_ptr<int> array{nullptr};
};

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");

    auto* data = manager.construct<my_data>("data")();
    data->len = 10;
    data->array = static_cast<int*>(manager.allocate(data->len * sizeof(int)));
    for (int i = 0; i < data->len; ++i) data->array[i] = i;
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");

    auto* data = manager.find<my_data>("data").first;
    for (int i = 0; i < data->len; ++i)
      std::cout << data->array[i] << std::endl;

    manager.deallocate(data->array.get());
    manager.destroy_ptr(data);
  }

  return 0;
}