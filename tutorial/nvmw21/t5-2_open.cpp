// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp>

// Please execute t5-2_create beforehand.

int main() {

  if (metall::manager::consistent("/tmp/dir")) {
    metall::manager manager(metall::open_read_only, "/tmp/dir");
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;
  } else {
    // This line will be executed since "/tmp/dir" was not closed properly.
    std::cerr << "Inconsistent data --- /tmp/dir was not closed properly" << std::endl;
  }

  if (metall::manager::consistent("/tmp/snapshot")) {
    metall::manager manager(metall::open_read_only, "/tmp/snapshot");
    std::cout << "Opened /tmp/snapshot" << std::endl;
    auto *n = manager.find<int>("n").first;
    std::cout << *n << std::endl;
  } else {
    std::cerr << "Inconsistent data --- /tmp/snapshot was not closed properly" << std::endl;
  }

  return 0;
}