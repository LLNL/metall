// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <filesystem>

#include <metall/utility/metall_mpi_datastore.hpp>
#include <metall/utility/datastore_ls.hpp>

int main(int argc, char *argv[]) {
  if (argc <= 1) {
    std::cerr << "Empty datastore path" << std::endl;
    std::abort();
  }

  const std::filesystem::path datastore_path = argv[1];
  const int mpi_rank = (argc < 3) ? 0 : std::stoi(argv[2]);

  const auto local_datastore_path =
      metall::utility::mpi_datastore::make_local_dir_path(datastore_path,
                                                          mpi_rank);

  if (!metall::manager::consistent(local_datastore_path)) {
    std::cerr << "Inconsistent datastore or invalid datastore path"
              << std::endl;
    std::abort();
  }

  metall::utility::ls_named_object(local_datastore_path);
  std::cout << std::endl;

  metall::utility::ls_unique_object(local_datastore_path);
  std::cout << std::endl;

  metall::utility::ls_anonymous_object(local_datastore_path);
  std::cout << std::endl;

  return 0;
}