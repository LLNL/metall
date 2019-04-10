// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/file_path", 1ULL << 25);
    int* a = manager.construct<int>(metall::unique_instance)(10);
    manager.snapshot("/tmp/snapshot");
  }

  {
    metall::manager manager(metall::open_only, "/tmp/snapshot");
    int* a = manager.find<int>(metall::unique_instance).first;
    assert(*a == 10);
  }

  metall::manager::remove_file("/tmp/snapshot");
}