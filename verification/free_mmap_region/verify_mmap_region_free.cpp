// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <cstdlib>
#include <thread>
#include <chrono>

#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/memory.hpp>

#ifdef __linux__
#include <linux/falloc.h>  // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
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

void free_file_backed_mmap(
    const std::string &file_name, const std::size_t file_size,
    const std::function<std::pair<int, void *>(
        const std::string &file_name, const long unsigned int size)> &map_file,
    const std::function<void(void *const addr, const long unsigned int size)>
        &uncommit_function) {
  std::cout << "\n----- Map file -----" << std::endl;
  int fd = -1;
  void *addr = nullptr;
  std::tie(fd, addr) = map_file(file_name, file_size);
  close_file(fd);
  std::cout << "DRAM usage (GB)\t"
            << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)\t"
            << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;

  // Commit pages
  std::cout << "\n----- Commit Pages -----" << std::endl;
  char *const map = reinterpret_cast<char *>(addr);
  const auto page_size = get_page_size();
  for (uint64_t i = 0; i < file_size; i += page_size) {
    map[i] = 1;
  }
  std::cout << "DRAM usage (GB)\t"
            << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)\t"
            << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;

  std::cout << "\n----- Uncommit pages -----" << std::endl;
  for (std::size_t offset = 0; offset < file_size; offset += page_size) {
    uncommit_function(static_cast<char *>(addr) + offset, page_size);
  }
  std::cout << "The current file size\t"
            << mdtl::get_actual_file_size(file_name) << std::endl;
  std::cout << "DRAM usage (GB)\t"
            << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)\t"
            << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;

  std::cout << "\n----- munmap -----" << std::endl;
  unmap(addr, file_size);

  std::cout << "The current file size\t"
            << mdtl::get_actual_file_size(file_name) << std::endl;
  std::cout << "DRAM usage (GB)\t"
            << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)\t"
            << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;
}

int main() {
  check_macros();

  std::string file_name = "/tmp/file";
  const auto page_size = get_page_size();
  const std::size_t file_size = page_size * 1024ULL * 512;

  std::cout << "\n------------------------------" << std::endl;
  std::cout << "\nMap Shared" << std::endl;
  std::cout << "\n------------------------------" << std::endl;
  free_file_backed_mmap(file_name, file_size, map_file_share,
                        mdtl::uncommit_shared_pages_and_free_file_space);

  std::cout << "\n------------------------------" << std::endl;
  std::cout << "\nMap Private" << std::endl;
  std::cout << "\n------------------------------" << std::endl;
  free_file_backed_mmap(file_name, file_size, map_file_private,
                        mdtl::uncommit_private_nonanonymous_pages);

  return 0;
}