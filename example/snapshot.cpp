// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <string>

#include <metall/metall.hpp>

int main() {
  const std::string manager_path("/dev/shm/dir_path");
  const std::string local_snapshot_dir_prefix("/dev/shm/snapshot-");
  const std::string remote_snapshot_dir_prefix("/tmp/snapshot-");

  {
    // Just use Metall as usual
    metall::manager manager(metall::create_only, manager_path.c_str());

    int* a = manager.construct<int>(metall::unique_instance)(-1);

    for (int i = 0; i < 10; ++i) {
      *a = i;

      // This is a synchronous function call
      // It calls manager.sync() internally first and
      // copies the entire data creating new files with the prefix "/tmp/snapshot"
      const std::string snapshot_name = local_snapshot_dir_prefix + std::to_string(i);
      manager.snapshot(snapshot_name.c_str());

      // You can copy the snapshot to another place asynchronously
      // It returns a std::future object (see https://en.cppreference.com/w/cpp/thread/future)
      const std::string snapshot_copy_name = remote_snapshot_dir_prefix + std::to_string(i);
      auto handler = metall::manager::copy_async(snapshot_name.c_str(), snapshot_copy_name.c_str());

      // handler.get() waits until it finishes and returns true if the files were copied, false otherwise
      if (handler.get()) {
        metall::manager::remove(snapshot_name.c_str()); // Remove the snapshot, if needed
      }
    }
  }

  // Restart from tne snapshot
  {
    int snapshot_no = 8;

    // Copy a snapshot to a local space
    const std::string snapshot_copy_name = remote_snapshot_dir_prefix + std::to_string(snapshot_no);
    metall::manager::copy(snapshot_copy_name.c_str(), local_snapshot_dir_prefix.c_str());

    // Remove the snapshot, if needed
    metall::manager::remove(snapshot_copy_name.c_str());

    // Open the snapshot as normal file
    metall::manager manager(metall::open_only, local_snapshot_dir_prefix.c_str());
    int* a = manager.find<int>(metall::unique_instance).first;
    std::cout << *a << std::endl; // Print snapshot_no, i.e., 8
  }

  return 0;
}