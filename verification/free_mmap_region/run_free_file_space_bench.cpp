// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cassert>
#include <functional>
#include <thread>
#include <vector>

#include "free_mmap_region.hpp"

void commit_pages(const std::size_t size, void *const addr) {
  const std::size_t page_size = mdtl::get_page_size();
  assert(page_size > 0);
  assert(size % page_size == 0);

  const std::size_t num_pages = size / page_size;
  const auto num_threads = (int)std::min(
      (std::size_t)num_pages, (std::size_t)std::thread::hardware_concurrency());
  std::vector<std::thread *> threads(num_threads, nullptr);

  const auto start = mdtl::elapsed_time_sec();
  for (int t = 0; t < num_threads; ++t) {
    const auto range = mdtl::partial_range(num_pages, t, num_threads);
    threads[t] = new std::thread([range, page_size, addr]() {
      for (std::size_t p = range.first; p < range.second; ++p) {
        auto map = static_cast<char *>(addr);
        map[p * page_size] = 1;
      }
    });
  }
  for (auto &th : threads) {
    th->join();
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void free_file_space(const std::size_t size,
                     const std::function<void(const std::size_t size,
                                              void *const addr)> &free_function,
                     void *const addr) {
  const std::size_t page_size = mdtl::get_page_size();
  assert(page_size > 0);
  assert(size % page_size == 0);

  const auto start = mdtl::elapsed_time_sec();
  for (std::size_t offset = 0; offset < size; offset += page_size) {
    free_function(page_size, static_cast<char *>(addr) + offset);
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

int main(int, char *argv[]) {
  const int mode = std::stoul(argv[1]);

  const std::string file_path(argv[2]);
  assert(!file_path.empty());

  const std::size_t map_size = std::stoll(argv[3]);

  int fd = -1;
  void *map_addr = nullptr;
  std::tie(fd, map_addr) = map_file_share(file_path, map_size);
  std::cout << "DRAM usage (GB)"
            << "\t" << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL)
            << std::endl;
  std::cout << "DRAM cache usage (GB)"
            << "\t" << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;

  commit_pages(map_size, map_addr);
  sync_mmap(map_addr, map_size);
  std::cout << "DRAM usage (GB)"
            << "\t" << (double)mdtl::get_used_ram_size() / (1ULL << 30ULL)
            << std::endl;
  std::cout << "DRAM cache usage (GB)"
            << "\t" << (double)mdtl::get_page_cache_size() / (1ULL << 30ULL)
            << std::endl;

  if (mode == 0) {
    std::cout << "uncommit_shared_pages only" << std::endl;

    close_file(fd);
    free_file_space(
        map_size,
        [](const std::size_t free_size, void *const free_addr) {
          if (!mdtl::uncommit_shared_pages(free_addr, free_size)) {
            std::cerr << "Failed to uncommit page" << std::endl;
            std::abort();
          }
        },
        map_addr);
    sync_mmap(map_addr, map_size);
    sync_file(file_path);
  } else if (mode == 1) {
    std::cout << "uncommit_shared_pages and free_mmap_region" << std::endl;

    free_file_space(
        map_size,
        [fd, map_addr](const std::size_t free_size, void *const free_addr) {
          if (!mdtl::uncommit_shared_pages(free_addr, free_size)) {
            std::cerr << "Failed to uncommit page" << std::endl;
            std::abort();
          }
          const ssize_t offset =
              static_cast<char *>(free_addr) - static_cast<char *>(map_addr);
          if (!mdtl::free_file_space(fd, offset, free_size)) {
            std::cerr << "Failed to free file space" << std::endl;
            std::abort();
          }
        },
        map_addr);
    close_file(fd);
    sync_mmap(map_addr, map_size);
    sync_file(file_path);
  } else if (mode == 2) {
    std::cout << "uncommit_shared_pages_and_free_file_space" << std::endl;

    close_file(fd);
    free_file_space(
        map_size,
        [](const std::size_t free_size, void *const free_addr) {
          if (!mdtl::uncommit_shared_pages_and_free_file_space(free_addr,
                                                               free_size)) {
            std::cerr << "Failed to uncommit file backed page" << std::endl;
            std::abort();
          }
        },
        map_addr);
    sync_mmap(map_addr, map_size);
    sync_file(file_path);
  }

  unmap(map_addr, map_size);

  std::cout << "File size (GB)\t"
            << (double)mdtl::get_file_size(file_path) / (1ULL << 30ULL)
            << std::endl;
  std::cout << "Actual file size (GB)\t"
            << (double)mdtl::get_actual_file_size(file_path) / (1ULL << 30ULL)
            << std::endl;

  return 0;
}