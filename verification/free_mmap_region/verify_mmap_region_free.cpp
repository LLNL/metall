// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <cstdlib>
#include <thread>
#include <chrono>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/memory.hpp>

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include "free_mmap_region.hpp"

void check_macros() {
#ifndef FALLOC_FL_PUNCH_HOLE
  std::cerr << "FALLOC_FL_PUNCH_HOLE is not defined" << std::endl;
#endif

#ifndef MADV_FREE
  std::cerr << "MADV_FREE is not defined" << std::endl;
#endif

#ifndef MADV_REMOVE
  std::cerr << "MADV_REMOVE is not defined" << std::endl;
#endif

#ifndef FALLOC_FL_PUNCH_HOLE
  std::cerr << "FALLOC_FL_PUNCH_HOLE is not defined" << std::endl;
#endif

#ifndef FALLOC_FL_KEEP_SIZE
  std::cerr << "FALLOC_FL_KEEP_SIZE is not defined" << std::endl;
#endif
}

void setup_file(const std::string &file_name, const std::size_t file_size) {
  int fd = -1;
  void *addr = nullptr;
  std::tie(fd, addr) = map_file(file_name, file_size);
  close_file(fd);

  // Commit only half pages
  char *const map = reinterpret_cast<char *>(addr);
  const auto page_size = get_page_size();
  for (uint64_t i = 0; i < file_size; i += page_size) {
    map[i] = 1;
  }

  std::cout << "\nmunmap" << std::endl;
  unmap(addr, file_size);

  std::cout << "Expected file size\t" << file_size
            << "\nThe current file size\t" << util::get_actual_file_size(file_name) << std::endl;
}

void free_file_backed_mmap(const std::string &file_name, const std::size_t file_size,
                           const std::function<void(const std::size_t size, void *const addr)> &free_function) {
  int fd = -1;
  void *addr = nullptr;
  std::tie(fd, addr) = map_file(file_name, file_size);
  close_file(fd);

  const auto page_size = get_page_size();
  for (std::size_t offset = 0; offset < file_size; offset += page_size) {
    free_function(page_size, static_cast<char *>(addr) + offset);
  }
  std::cout << "The current file size\t" << util::get_actual_file_size(file_name) << std::endl;

  std::cout << "\nmunmap" << std::endl;
  unmap(addr, file_size);

  std::cout << "The current file size\t" << util::get_actual_file_size(file_name) << std::endl;
}

int main() {
  const auto page_size = get_page_size();
  const std::size_t file_size = page_size * 1024ULL;

  setup_file("", file_size);
}