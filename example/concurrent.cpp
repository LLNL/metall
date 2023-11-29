// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file concurrent.cpp
/// \brief This example demonstrates Metall's concurrency support.
/// Metall can be used in a multi-threaded environment.
/// Please see the API documentation of the manager class to find out which
/// functions are thread-safe.

#include <iostream>
#include <thread>

#include <metall/metall.hpp>

void metall_alloc(metall::manager& manager, const int tid) {
  for (int i = 0; i < 10; ++i) {
    if (tid % 2 == 0) {
      manager.deallocate(manager.allocate(10));
    } else {
      manager.destroy_ptr(manager.construct<int>(metall::anonymous_instance)());
    }
  }
}

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");

    {
      std::thread t1(metall_alloc, std::ref(manager), 1);
      std::thread t2(metall_alloc, std::ref(manager), 2);
      std::thread t3(metall_alloc, std::ref(manager), 3);
      std::thread t4(metall_alloc, std::ref(manager), 4);

      t1.join();
      t2.join();
      t3.join();
      t4.join();
    }
    assert(manager.check_sanity());
    assert(manager.all_memory_deallocated());
  }

  return 0;
}