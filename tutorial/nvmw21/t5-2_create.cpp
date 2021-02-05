// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp>

int main() {

  metall::manager manager(metall::create_only, "/tmp/dir");
  auto *n = manager.construct<int>("n")();
  *n = 10;

  manager.snapshot("/tmp/snapshot");

  *n = 20;
  std::abort(); // Assumes that a fatal error happens here, i.e., Metall "/tmp/dir" is not closed properly.

  return 0;
}