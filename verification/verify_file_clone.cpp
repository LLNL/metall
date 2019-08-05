// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/file_clone.hpp>
#include <metall/detail/utility/mmap.hpp>

namespace util = metall::detail::utility;

void init_file(const std::string &file_path, const std::size_t size) {

  util::create_file(file_path);
  util::extend_file_size(file_path, size);

  auto ret = util::map_file_write_mode(file_path, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(ret.second);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    map[i] = i;
  }

  util::munmap(ret.first, ret.second, size, true);
}

void update_file(const std::string &file_path,
                 const std::size_t size,
                 const std::size_t offset,
                 const std::size_t update_value) {

  auto ret = util::map_file_write_mode(file_path, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(ret.second);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    map[i] = i + update_value;
  }

  util::munmap(ret.first, ret.second, size, true);
}

void validate_file(const std::string &file_path,
                   const std::size_t size,
                   const std::size_t offset,
                   const std::size_t update_value) {

  auto ret = util::map_file_read_mode(file_path, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(ret.second);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    if (map[i] != i + update_value) {
      std::cerr << "Invalid value at " << i << std::endl;
      std::abort();
    }
  }

  util::munmap(ret.first, ret.second, size, false);
}

int main(int argc, char *argv[]) {

  const std::string source_file_path(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);
  const std::string destination_file_path(argv[3]);

  util::remove_file(source_file_path);
  util::remove_file(destination_file_path);

  init_file(source_file_path, file_size);

  if (!util::clone_file(source_file_path, destination_file_path, true)) {
    std::cerr << "Failed to clone file: " << source_file_path << " to " << destination_file_path << std::endl;
    std::abort();
  }

  validate_file(destination_file_path, file_size, 0, 0);

  std::cout << source_file_path
            << "\nfile size = " << util::get_file_size(source_file_path)
            << "\nactual file size = " << util::get_actual_file_size(source_file_path) << std::endl;

  std::cout << destination_file_path
            << "\nfile size = " << util::get_file_size(destination_file_path)
            << "\nactual file size = " << util::get_actual_file_size(destination_file_path) << std::endl;

  update_file(destination_file_path, file_size / 4ULL, file_size / 2ULL, 123);
  validate_file(destination_file_path, file_size / 4ULL, file_size / 2ULL, 123);
  std::cout << destination_file_path
            << "\nfile size = " << util::get_file_size(destination_file_path)
            << "\nactual file size = " << util::get_actual_file_size(destination_file_path) << std::endl;

  return 0;
}