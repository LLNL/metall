// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <cassert>
#include <functional>
#include <thread>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/time.hpp>
#include <metall/detail/utility/memory.hpp>
#include <metall/detail/utility/common.hpp>

namespace util = metall::detail::utility;

static constexpr int k_map_nosync =
#ifdef MAP_NOSYNC
    MAP_NOSYNC;
#else
    0;
#warning "MAP_NOSYNC is not defined"
#endif

std::pair<int, void *> map_file(const std::string &file_path, const std::size_t size) {
  const auto start = util::elapsed_time_sec();

  std::cout << "size: " << size << std::endl;

  if (!util::create_file(file_path) || !util::extend_file_size(file_path, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_path << std::endl;
    std::abort();
  }

  const auto ret = util::map_file_write_mode(file_path, nullptr, size, 0, k_map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }

  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return ret;
}

void commit_pages(const std::size_t size, void *const addr) {
  const std::size_t page_size = util::get_page_size();
  assert(page_size > 0);
  assert(size % page_size == 0);

  const std::size_t num_pages = size / page_size;
  const auto num_threads = (int)std::min((std::size_t)num_pages, (std::size_t)std::thread::hardware_concurrency());
  std::vector<std::thread *> threads(num_threads, nullptr);

  const auto start = util::elapsed_time_sec();
  for (int t = 0; t < num_threads; ++t) {
    const auto range = util::partial_range(num_pages, t, num_threads);
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
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void free_file_space(const std::size_t size,
                     const std::function<void(const std::size_t size, void *const addr)> &free_function,
                     void *const addr) {
  const std::size_t page_size = util::get_page_size();
  assert(page_size > 0);
  assert(size % page_size == 0);

  const auto start = util::elapsed_time_sec();
  for (std::size_t offset = 0; offset < size; offset += page_size) {
    free_function(page_size, static_cast<char *>(addr) + offset);
  }
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void os_msync(void *const addr, const std::size_t size) {
  const auto start = util::elapsed_time_sec();
  util::os_msync(addr, size, true);
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void close_file(const int fd) {
  const auto start = util::elapsed_time_sec();
  ::close(fd);
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void os_fsync(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }

  const auto start = util::elapsed_time_sec();
  util::os_fsync(fd);
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void unmap(void *const addr, const std::size_t size) {
  const auto start = util::elapsed_time_sec();

  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }

  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

int main(int, char *argv[]) {

  const int mode = std::stoul(argv[1]);

  const std::string file_path(argv[2]);
  assert(!file_path.empty());

  const std::size_t map_size = std::stoll(argv[3]);

  int fd = -1;
  void *map_addr = nullptr;
  std::tie(fd, map_addr) = map_file(file_path, map_size);
  std::cout << "DRAM usage (GB)" << "\t" << (double)util::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)" << "\t" << (double)util::get_page_cache_size() / (1ULL << 30ULL) << std::endl;

  commit_pages(map_size, map_addr);
  os_msync(map_addr, map_size);
  std::cout << "DRAM usage (GB)" << "\t" << (double)util::get_used_ram_size() / (1ULL << 30ULL) << std::endl;
  std::cout << "DRAM cache usage (GB)" << "\t" << (double)util::get_page_cache_size() / (1ULL << 30ULL) << std::endl;

  if (mode == 0) {
    std::cout << "uncommit_shared_pages only" << std::endl;

    close_file(fd);
    free_file_space(map_size,
                    [](const std::size_t free_size, void *const free_addr) {
                      if (!util::uncommit_shared_pages(free_addr, free_size)) {
                        std::cerr << "Failed to uncommit page" << std::endl;
                        std::abort();
                      }
                    },
                    map_addr);
    os_msync(map_addr, map_size);
    os_fsync(file_path);
  } else if (mode == 1) {
    std::cout << "uncommit_shared_pages and free_file_space" << std::endl;

    free_file_space(map_size,
                    [fd, map_addr](const std::size_t free_size, void *const free_addr) {
                      if (!util::uncommit_shared_pages(free_addr, free_size)) {
                        std::cerr << "Failed to uncommit page" << std::endl;
                        std::abort();
                      }
                      const ssize_t offset = static_cast<char *>(free_addr) - static_cast<char *>(map_addr);
                      if (!util::free_file_space(fd, offset, free_size)) {
                        std::cerr << "Failed to free file space" << std::endl;
                        std::abort();
                      }
                    },
                    map_addr);
    close_file(fd);
    os_msync(map_addr, map_size);
    os_fsync(file_path);
  } else if (mode == 2) {
    std::cout << "uncommit_file_backed_pages" << std::endl;

    close_file(fd);
    free_file_space(map_size,
                    [](const std::size_t free_size, void *const free_addr) {
                      if (!util::uncommit_file_backed_pages(free_addr, free_size)) {
                        std::cerr << "Failed to uncommit file backed page" << std::endl;
                        std::abort();
                      }
                    },
                    map_addr);
    os_msync(map_addr, map_size);
    os_fsync(file_path);
  }

  unmap(map_addr, map_size);

  std::cout << "File size (GB)\t" << (double)util::get_file_size(file_path) / (1ULL << 30ULL) << std::endl;
  std::cout << "Actual file size (GB)\t" << (double)util::get_actual_file_size(file_path) / (1ULL << 30ULL) << std::endl;

  return 0;
}