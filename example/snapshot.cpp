// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <string>

#include <metall/metall.hpp>

int main() {
  const std::string master_path("/tmp/dir_path");
  const std::string snapshot_dir_prefix("/tmp/snapshot-");
  const std::string snapshot_name0 = snapshot_dir_prefix + "ver0";
  const std::string snapshot_name1 = snapshot_dir_prefix + "ver1";

  {
    // Create the master data
    metall::manager manager(metall::create_only, master_path.c_str());
    int *a = manager.construct<int>(metall::unique_instance)(0);

    // Take a snapshot before updating to '1'
    manager.snapshot(snapshot_name0.c_str());
    *a = 1;

    // Take a snapshot before updating to '2'
    manager.snapshot(snapshot_name1.c_str());
    *a = 2;
  }

  // Open snapshot 0 if it is consistent, i.e., it was closed properly.
  if (metall::manager::consistent(snapshot_name0.c_str())) {
    metall::manager manager(metall::open_read_only, snapshot_name0.c_str());
    int *a = manager.find<int>(metall::unique_instance).first;
    std::cout << *a << std::endl;  // Print 0
  } else {
    std::cerr << snapshot_name0 << " is inconsistent" << std::endl;
  }

  // Open snapshot 1 if it is consistent, i.e., it was closed properly.
  if (metall::manager::consistent(snapshot_name1.c_str())) {
    metall::manager manager(metall::open_read_only, snapshot_name1.c_str());
    int *a = manager.find<int>(metall::unique_instance).first;
    std::cout << *a << std::endl;  // Print 1
  } else {
    std::cerr << snapshot_name1 << " is inconsistent" << std::endl;
  }

  // Open the master data if it is consistent, i.e., it was closed properly.
  if (metall::manager::consistent(master_path.c_str())) {
    metall::manager manager(metall::open_read_only, master_path.c_str());
    int *a = manager.find<int>(metall::unique_instance).first;
    std::cout << *a << std::endl;  // Print 2
  } else {
    std::cerr << master_path << " is inconsistent" << std::endl;
  }

  return 0;
}