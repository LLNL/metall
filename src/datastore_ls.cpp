// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <filesystem>

#include <metall/utility/datastore_ls.hpp>

int main(int argc, char *argv[]) {
  if (argc == 1) {
    std::cerr << "Empty datastore path" << std::endl;
    std::abort();
  }

  const std::filesystem::path datastore_path = argv[1];

  metall::utility::ls_named_object(datastore_path);
  std::cout << std::endl;

  metall::utility::ls_unique_object(datastore_path);
  std::cout << std::endl;

  metall::utility::ls_anonymous_object(datastore_path);
  std::cout << std::endl;

  return 0;
}