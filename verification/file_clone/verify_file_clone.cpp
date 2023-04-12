// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/detail/file.hpp>
#include <metall/detail/file_clone.hpp>
#include <metall/detail/mmap.hpp>

namespace mdtl = metall::mtlldetail;

void init_file(const std::string &file_path, const std::size_t size) {
  mdtl::create_file(file_path);
  mdtl::extend_file_size(file_path, size);

  auto ret = mdtl::map_file_write_mode(file_path, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(ret.second);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    map[i] = i;
  }

  mdtl::munmap(ret.first, ret.second, size, true);
}

void update_file(const std::string &file_path, const std::size_t size,
                 const std::size_t update_value) {
  int fd;
  void *addr;
  std::tie(fd, addr) = mdtl::map_file_write_mode(file_path, nullptr, size, 0);
  if (fd == -1 || !addr) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(addr);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    map[i] = i + update_value;
  }

  if (!mdtl::munmap(fd, addr, size, true)) {
    std::abort();
  }
}

void validate_file(const std::string &file_path, const std::size_t size,
                   const std::size_t update_value) {
  auto ret = mdtl::map_file_read_mode(file_path, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::abort();
  }

  auto map = static_cast<std::size_t *>(ret.second);

  for (std::size_t i = 0; i < size / sizeof(std::size_t); ++i) {
    if (map[i] != i + update_value) {
      std::cerr << "Invalid value at " << i << ": has to be "
                << i + update_value << " instead of " << map[i] << std::endl;
      std::abort();
    }
  }

  if (!mdtl::munmap(ret.first, ret.second, size, false)) {
    std::abort();
  }
}

// FIXME: check sparse copy
int main([[maybe_unused]] int argc, char *argv[]) {
  const std::string source_file_path(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);
  const std::string destination_file_path(argv[3]);

  mdtl::remove_file(source_file_path);
  mdtl::remove_file(destination_file_path);

  std::cout << "Init the source file" << std::endl;
  init_file(source_file_path, file_size);

  std::cout << "\nClone the file" << std::endl;
  if (!mdtl::clone_file(source_file_path, destination_file_path)) {
    std::cerr << "Failed to clone file: " << source_file_path << " to "
              << destination_file_path << std::endl;
    std::abort();
  }

  std::cout << "Validate the clone file" << std::endl;
  validate_file(destination_file_path, file_size, 0);

  std::cout << "\nUpdate the source file" << std::endl;
  update_file(source_file_path, file_size, 1);
  std::cout << "Validate the source file" << std::endl;
  validate_file(source_file_path, file_size, 1);
  std::cout << "Validate the clone file (to make sure there is no affect to "
               "the clone file)"
            << std::endl;
  validate_file(destination_file_path, file_size, 0);

  std::cout << "\nUpdate the clone file" << std::endl;
  update_file(destination_file_path, file_size, 2);
  std::cout << "Validate the clone file" << std::endl;
  validate_file(destination_file_path, file_size, 2);
  std::cout << "Validate the source file (to make sure there is no affect to "
               "the source file)"
            << std::endl;
  validate_file(source_file_path, file_size, 1);

  return 0;
}