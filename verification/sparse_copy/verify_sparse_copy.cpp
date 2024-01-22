// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <filesystem>

#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>

namespace mdtl = metall::mtlldetail;

int main() {
  const std::filesystem::path src_path = "source.dat";
  const ssize_t size = 1024 * 1024 * 32;

  if (!mdtl::create_file(src_path)) {
    std::cerr << "Failed to create a file" << std::endl;
    return EXIT_FAILURE;
  }

  if (!mdtl::extend_file_size(src_path, size)) {
    std::cerr << "Failed to extend file size" << std::endl;
    return EXIT_FAILURE;
  }

  {
    auto [fd, map] = mdtl::map_file_write_mode(src_path, nullptr, size, 0);
    if (!map) {
      std::cerr << "Failed to map a file" << std::endl;
      return EXIT_FAILURE;
    }
    auto* buf = reinterpret_cast<char*>(map);
    buf[0] = 1;
    buf[1024 * 1024 - 1] = 1;
    if (!mdtl::munmap(fd, map, size, true)) {
      std::cerr << "Failed to unmap a file" << std::endl;
      return EXIT_FAILURE;
    }
  }

  if (mdtl::get_file_size(src_path) < size) {
    std::cerr << "Failed to extend file size" << std::endl;
    return EXIT_FAILURE;
  }

  if (mdtl::get_actual_file_size(src_path) >= size) {
    std::cerr << "Failed to create a sparse file" << std::endl;
    std::cerr << "Actual file size: " << mdtl::get_actual_file_size(src_path)
              << std::endl;
    return EXIT_FAILURE;
  }

  // Sparse copy
  const std::filesystem::path dst_path = "destination.dat";
  if (!mdtl::fcpdtl::copy_file_sparse_manually(src_path, dst_path)) {
    std::cerr << "Failed to copy a file" << std::endl;
    return EXIT_FAILURE;
  }

  std::cout << "Source file size: " << mdtl::get_file_size(src_path)
            << std::endl;
  std::cout << "Destination file size: " << mdtl::get_file_size(dst_path)
            << std::endl;

  // Show actual size
  std::cout << "Source actual file size: "
            << mdtl::get_actual_file_size(src_path) << std::endl;
  std::cout << "Destination actual file size: "
            << mdtl::get_actual_file_size(dst_path) << std::endl;

  //  if (mdtl::get_file_size(dst_path) < size) {
  //    std::cerr << "Failed to extend file size" << std::endl;
  //    return EXIT_FAILURE;
  //  }

//  if (mdtl::get_actual_file_size(dst_path) >= size) {
//    std::cerr << "Failed to create a sparse file" << std::endl;
//    std::cerr << "Actual file size: " << mdtl::get_actual_file_size(dst_path)
//              << std::endl;
//    return EXIT_FAILURE;
//  }

  return 0;
}