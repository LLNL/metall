// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This program shows the crash consistency of Metall snapshot

#include <iostream>
#include <metall/metall.hpp>

int main() {
  metall::manager manager(metall::create_only, "/tmp/dir");
  auto *n = manager.construct<int>("n")();
  *n = 10;

  manager.snapshot("/tmp/snapshot");
  std::cout << "Created a snapshot" << std::endl;

  *n = 20;
  std::cout << "Going to abort (simulating a fatal error)" << std::endl;
  std::abort();  // Assumes that a fatal error happens here, i.e., Metall
                 // "/tmp/dir" is not closed properly.

  return 0;
}