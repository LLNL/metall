// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/file_clone.hpp>

namespace util = metall::detail::utility;

void init_file(const std::string &file_path, const std::size_t size) {

  std::ofstream ofs(file_path, std::ofstream::out | std::ofstream::binary);
  if (!ofs.is_open()) {
    std::cerr << "Cannot open file: " << file_path << std::endl;
    std::abort();
  }

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    if (!ofs.write(reinterpret_cast<char *>(&i), sizeof(std::size_t))) {
      std::cerr << "Failed to write to " << file_path << std::endl;
      std::abort();
    }
  }

  ofs.close();
}

void update_file(const std::string &file_path, const std::size_t size) {

  std::ofstream ofs(file_path, std::ofstream::out | std::ofstream::binary | std::ofstream::ate);
  if (!ofs.is_open()) {
    std::cerr << "Cannot open file: " << file_path << std::endl;
    std::abort();
  }

  if (!ofs.seekp(0, std::ofstream::beg)) {
    std::cerr << "Cannot seek in file: " << file_path << std::endl;
    std::abort();
  }

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    if (!ofs.write(reinterpret_cast<char *>(&i), sizeof(std::size_t))) {
      std::cerr << "Failed to write to " << file_path << std::endl;
      std::abort();
    }
  }

  ofs.close();
}

int main(int argc, char *argv[]) {

  const std::string source_file_path(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);
  const std::string destination_file_path(argv[3]);

  init_file(source_file_path, file_size);

  if (!util::clone_file(source_file_path, destination_file_path, true)) {
    std::cerr << "Failed to clone file: " << source_file_path << " to " << destination_file_path << std::endl;
    std::abort();
  }

  std::cout << source_file_path
            << "\nfile size = " << util::get_file_size(source_file_path)
            << "\nactual file size = " << util::get_actual_file_size(source_file_path) << std::endl;

  std::cout << destination_file_path
            << "\nfile size = " << util::get_file_size(destination_file_path)
            << "\nactual file size = " << util::get_actual_file_size(destination_file_path) << std::endl;

  update_file(destination_file_path, file_size / 2ULL);
  std::cout << destination_file_path
            << "\nfile size = " << util::get_file_size(destination_file_path)
            << "\nactual file size = " << util::get_actual_file_size(destination_file_path) << std::endl;

  return 0;
}