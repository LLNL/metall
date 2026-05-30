// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#include <metall/utility/metall_mpi_adaptor.hpp>

namespace {

namespace fs = std::filesystem;

using adaptor_type = metall::utility::metall_mpi_adaptor;

fs::path test_root_dir(const int world_size) {
  const char *env_path = std::getenv("METALL_TEST_DIR");
  const fs::path base_dir =
      env_path ? fs::path(env_path) : fs::path("/tmp/metall_test_dir");
  return base_dir /
         (std::string("metall_mpi_adaptor_test_") + std::to_string(world_size));
}

void log_failure(const int rank, const std::string &message) {
  std::cerr << "[metall_mpi_adaptor_test rank " << rank << "] " << message
            << std::endl;
}

bool sync_success(const bool local_success, const MPI_Comm &comm,
                  const int rank, const std::string &phase) {
  const auto result =
      metall::utility::mpi::global_logical_and(local_success, comm);
  if (!result.first) {
    if (rank == 0) {
      std::cerr << "Failed to synchronize MPI test status at phase: " << phase
                << std::endl;
    }
    return false;
  }

  if (!result.second && rank == 0) {
    std::cerr << "Phase failed: " << phase << std::endl;
  }
  return result.second;
}

bool ensure_directory(const fs::path &path, const int rank) {
  if (rank != 0) {
    return true;
  }

  std::error_code error;
  fs::create_directories(path, error);
  if (error) {
    std::cerr << "Failed to create directory " << path << ": "
              << error.message() << std::endl;
    return false;
  }
  return true;
}

bool force_remove_all(const fs::path &path, const MPI_Comm &comm,
                      const int rank) {
  bool local_success = true;
  if (rank == 0) {
    std::error_code error;
    fs::remove_all(path, error);
    if (error) {
      std::cerr << "Failed to remove " << path << ": " << error.message()
                << std::endl;
      local_success = false;
    }
  }

  if (::MPI_Barrier(comm) != MPI_SUCCESS) {
    return false;
  }
  return sync_success(local_success, comm, rank,
                      std::string("cleanup ") + path.string());
}

bool expect_path_eq(const fs::path &actual, const fs::path &expected,
                    const int rank, const std::string &label) {
  if (actual == expected) {
    return true;
  }

  log_failure(rank, label + " mismatch: actual=" + actual.string() +
                        " expected=" + expected.string());
  return false;
}

bool expect_true(const bool value, const int rank, const std::string &label) {
  if (value) {
    return true;
  }

  log_failure(rank, label);
  return false;
}

bool expect_equal(const int actual, const int expected, const int rank,
                  const std::string &label) {
  if (actual == expected) {
    return true;
  }

  log_failure(rank, label + " mismatch: actual=" + std::to_string(actual) +
                        " expected=" + std::to_string(expected));
  return false;
}

bool verify_rank_payload(const adaptor_type &adaptor, const int rank,
                         const int world_size, const int tag) {
  bool ok = true;
  const auto &manager = adaptor.get_local_manager();

  const auto rank_value = manager.find<int>("rank_value").first;
  ok &= expect_true(rank_value != nullptr, rank, "rank_value object missing");
  if (rank_value) {
    ok &= expect_equal(*rank_value, rank, rank, "rank_value");
  }

  const auto size_value = manager.find<int>("world_size").first;
  ok &= expect_true(size_value != nullptr, rank, "world_size object missing");
  if (size_value) {
    ok &= expect_equal(*size_value, world_size, rank, "world_size");
  }

  const auto tag_value = manager.find<int>("tag_value").first;
  ok &= expect_true(tag_value != nullptr, rank, "tag_value object missing");
  if (tag_value) {
    ok &= expect_equal(*tag_value, tag + rank, rank, "tag_value");
  }

  return ok;
}

bool write_rank_payload(adaptor_type &adaptor, const int rank,
                        const int world_size, const int tag) {
  bool ok = true;
  auto &manager = adaptor.get_local_manager();

  auto *rank_value = manager.construct<int>("rank_value")();
  ok &= expect_true(rank_value != nullptr, rank,
                    "failed to construct rank_value");
  if (rank_value) {
    *rank_value = rank;
  }

  auto *size_value = manager.construct<int>("world_size")();
  ok &= expect_true(size_value != nullptr, rank,
                    "failed to construct world_size");
  if (size_value) {
    *size_value = world_size;
  }

  auto *tag_value = manager.construct<int>("tag_value")();
  ok &=
      expect_true(tag_value != nullptr, rank, "failed to construct tag_value");
  if (tag_value) {
    *tag_value = tag + rank;
  }

  return ok;
}

bool verify_static_paths(const fs::path &root_path, const int rank) {
  bool ok = true;
  const auto root_generator = [root_path]() { return root_path; };
  ok &= expect_path_eq(adaptor_type::root_datastore_path(root_generator),
                       root_path, rank, "static root_datastore_path");
  ok &= expect_path_eq(
      adaptor_type::local_datastore_path(root_generator, rank),
      root_path / (std::string("subdir-") + std::to_string(rank)), rank,
      "static local_datastore_path");
  return ok;
}

bool verify_instance_paths(const adaptor_type &adaptor,
                           const fs::path &root_path, const int rank) {
  bool ok = true;
  ok &= expect_path_eq(adaptor.root_datastore_path(), root_path, rank,
                       "instance root_datastore_path");
  ok &= expect_path_eq(
      adaptor.local_datastore_path(),
      root_path / (std::string("subdir-") + std::to_string(rank)), rank,
      "instance local_datastore_path");
  return ok;
}

bool verify_partition_metadata(const fs::path &root_path, const MPI_Comm &comm,
                               const int rank, const int world_size,
                               const std::string &label) {
  bool ok = true;
  ok &= expect_equal(adaptor_type::partitions(root_path, comm), world_size,
                     rank, label + " partitions");
  ok &= expect_true(adaptor_type::consistent(root_path, comm), rank,
                    label + " consistent");
  return ok;
}

bool verify_open_only(const fs::path &root_path, const MPI_Comm &comm,
                      const int rank, const int world_size, const int tag,
                      const std::string &label) {
  bool ok = true;
  adaptor_type adaptor(metall::open_only, root_path, comm);
  ok &= verify_instance_paths(adaptor, root_path, rank);
  ok &= verify_rank_payload(adaptor, rank, world_size, tag);
  return sync_success(ok, comm, rank, label + " open_only");
}

bool verify_open_read_only(const fs::path &root_path, const MPI_Comm &comm,
                           const int rank, const int world_size, const int tag,
                           const std::string &label) {
  bool ok = true;
  const adaptor_type adaptor(metall::open_read_only, root_path, comm);
  ok &= verify_instance_paths(adaptor, root_path, rank);
  ok &= verify_rank_payload(adaptor, rank, world_size, tag);
  return sync_success(ok, comm, rank, label + " open_read_only");
}

bool test_primary_datastore(const fs::path &root_path,
                            const fs::path &copy_path,
                            const fs::path &snapshot_path, const MPI_Comm &comm,
                            const int rank, const int world_size) {
  bool ok = true;
  const auto root_generator = [root_path]() { return root_path; };
  ok &= verify_static_paths(root_path, rank);
  ok &= expect_true(adaptor_type::remove(root_path, comm), rank,
                    "remove should succeed for missing datastore");
  if (!sync_success(ok, comm, rank, "primary pre-create checks")) {
    return false;
  }

  {
    adaptor_type adaptor(metall::create_only, root_generator, comm);
    ok &= verify_instance_paths(adaptor, root_path, rank);
    ok &= write_rank_payload(adaptor, rank, world_size, 1000);
  }
  if (!sync_success(ok, comm, rank, "primary create_only")) {
    return false;
  }

  ok = verify_partition_metadata(root_path, comm, rank, world_size, "primary");
  if (!sync_success(ok, comm, rank, "primary metadata")) {
    return false;
  }

  if (!verify_open_only(root_path, comm, rank, world_size, 1000, "primary")) {
    return false;
  }
  if (!verify_open_read_only(root_path, comm, rank, world_size, 1000,
                             "primary")) {
    return false;
  }

  {
    adaptor_type adaptor(metall::open_only, root_path, comm);
    ok = adaptor.snapshot(snapshot_path, false);
  }
  if (!sync_success(ok, comm, rank, "snapshot initial")) {
    return false;
  }

  {
    adaptor_type adaptor(metall::open_only, root_path, comm);
    ok = adaptor.snapshot(snapshot_path, true);
  }
  if (!sync_success(ok, comm, rank, "snapshot overwrite")) {
    return false;
  }

  ok = verify_partition_metadata(snapshot_path, comm, rank, world_size,
                                 "snapshot");
  if (!sync_success(ok, comm, rank, "snapshot metadata")) {
    return false;
  }
  if (!verify_open_read_only(snapshot_path, comm, rank, world_size, 1000,
                             "snapshot")) {
    return false;
  }

  ok = adaptor_type::copy(root_generator, copy_path, comm, false);
  if (!sync_success(ok, comm, rank, "copy initial")) {
    return false;
  }

  ok = adaptor_type::copy(root_path, copy_path, comm, true);
  if (!sync_success(ok, comm, rank, "copy overwrite")) {
    return false;
  }

  ok = verify_partition_metadata(copy_path, comm, rank, world_size, "copy");
  if (!sync_success(ok, comm, rank, "copy metadata")) {
    return false;
  }
  return verify_open_read_only(copy_path, comm, rank, world_size, 1000, "copy");
}

bool test_capacity_constructor(const fs::path &root_path, const MPI_Comm &comm,
                               const int rank, const int world_size) {
  bool ok = true;

  {
    adaptor_type adaptor(metall::create_only, root_path, 1ULL << 24ULL, comm,
                         false);
    ok &= verify_instance_paths(adaptor, root_path, rank);
    ok &= write_rank_payload(adaptor, rank, world_size, 2000);
  }
  if (!sync_success(ok, comm, rank, "capacity create_only")) {
    return false;
  }

  ok = verify_partition_metadata(root_path, comm, rank, world_size, "capacity");
  if (!sync_success(ok, comm, rank, "capacity metadata")) {
    return false;
  }
  if (!verify_open_read_only(root_path, comm, rank, world_size, 2000,
                             "capacity")) {
    return false;
  }

  {
    adaptor_type adaptor(metall::create_only, root_path, 1ULL << 24ULL, comm,
                         true);
    ok = write_rank_payload(adaptor, rank, world_size, 3000);
  }
  if (!sync_success(ok, comm, rank, "capacity overwrite create_only")) {
    return false;
  }

  ok = verify_partition_metadata(root_path, comm, rank, world_size,
                                 "capacity overwrite");
  if (!sync_success(ok, comm, rank, "capacity overwrite metadata")) {
    return false;
  }
  return verify_open_read_only(root_path, comm, rank, world_size, 3000,
                               "capacity overwrite");
}

bool remove_and_verify(const fs::path &root_path, const MPI_Comm &comm,
                       const int rank, const std::string &label) {
  bool ok = true;
  ok &= expect_true(adaptor_type::remove(root_path, comm), rank,
                    label + " remove existing");

  bool exists = false;
  if (rank == 0) {
    exists = fs::exists(root_path);
  }
  ok &= sync_success(!exists, comm, rank, label + " removed from filesystem");
  ok &= expect_true(adaptor_type::remove(root_path, comm), rank,
                    label + " remove missing");
  return sync_success(ok, comm, rank, label + " remove checks");
}

}  // namespace

int main(int argc, char **argv) {
  if (::MPI_Init(&argc, &argv) != MPI_SUCCESS) {
    std::cerr << "MPI_Init failed" << std::endl;
    return 1;
  }

  int rank = -1;
  int world_size = 0;
  ::MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  ::MPI_Comm_size(MPI_COMM_WORLD, &world_size);

  bool success = true;

  if (world_size != 2 && world_size != 4) {
    if (rank == 0) {
      std::cerr << "This test expects 2 or 4 MPI ranks, got " << world_size
                << std::endl;
    }
    success = false;
  }

  const fs::path root_dir = test_root_dir(world_size);
  const fs::path primary_root = root_dir / "primary";
  const fs::path copy_root = root_dir / "copy";
  const fs::path snapshot_root = root_dir / "snapshot";
  const fs::path capacity_root = root_dir / "capacity";

  success &= sync_success(ensure_directory(root_dir, rank), MPI_COMM_WORLD,
                          rank, "create test root directory");
  success &= force_remove_all(primary_root, MPI_COMM_WORLD, rank);
  success &= force_remove_all(copy_root, MPI_COMM_WORLD, rank);
  success &= force_remove_all(snapshot_root, MPI_COMM_WORLD, rank);
  success &= force_remove_all(capacity_root, MPI_COMM_WORLD, rank);

  if (success) {
    success &= test_primary_datastore(primary_root, copy_root, snapshot_root,
                                      MPI_COMM_WORLD, rank, world_size);
  }
  if (success) {
    success &= test_capacity_constructor(capacity_root, MPI_COMM_WORLD, rank,
                                         world_size);
  }
  if (success) {
    success &= remove_and_verify(copy_root, MPI_COMM_WORLD, rank, "copy");
    success &=
        remove_and_verify(snapshot_root, MPI_COMM_WORLD, rank, "snapshot");
    success &= remove_and_verify(primary_root, MPI_COMM_WORLD, rank, "primary");
    success &=
        remove_and_verify(capacity_root, MPI_COMM_WORLD, rank, "capacity");
  }

  force_remove_all(copy_root, MPI_COMM_WORLD, rank);
  force_remove_all(snapshot_root, MPI_COMM_WORLD, rank);
  force_remove_all(primary_root, MPI_COMM_WORLD, rank);
  force_remove_all(capacity_root, MPI_COMM_WORLD, rank);

  ::MPI_Finalize();
  return success ? 0 : 1;
}