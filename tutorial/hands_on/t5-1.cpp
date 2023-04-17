// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows how to use the snapshot feature in Metall

#include <iostream>
#include <metall/metall.hpp>

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/dir");
    auto *n = manager.construct<int>("n")();
    *n = 10;

    // Create a snapshot
    manager.snapshot("/tmp/snapshot");
    std::cout << "Created a snapshot" << std::endl;

    *n = 20;
  }

  // Reattach the original data
  {
    metall::manager manager(metall::open_only, "/tmp/dir");
    std::cout << "Opened the original data" << std::endl;
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;  // Show '20'
  }

  // Reattach the snapshot
  // A snapshot can be used as a normal Metall data store
  {
    metall::manager manager(metall::open_only, "/tmp/snapshot");
    std::cout << "Opened the snapshot" << std::endl;
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;  // Show '10' since this snapshot was created
                                   // before assigning 20.
  }

  return 0;
}