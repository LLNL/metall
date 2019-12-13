// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_FREE_MEMORY_SPACE_HPP
#define METALL_FREE_MEMORY_SPACE_HPP

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

std::size_t get_page_size() {
  const auto page_size = util::get_page_size();
  if (page_size <= 0) {
    std::cerr << __LINE__ << " Failed to get the page size" << std::endl;
    std::abort();
  }
  return (std::size_t)page_size;
}

std::pair<int, void *> map_file(const std::string &file_path, const std::size_t size) {
  const auto start = util::elapsed_time_sec();

  std::cout << "Map size: " << size << std::endl;

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

void unmap(void *const addr, const std::size_t size) {
  const auto start = util::elapsed_time_sec();

  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }

  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void sync_mmap(void *const addr, const std::size_t size) {
  const auto start = util::elapsed_time_sec();
  util::os_msync(addr, size, true);
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void sync_file(const std::string &path) {
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

void close_file(const int fd) {
  const auto start = util::elapsed_time_sec();
  if (!util::os_close(fd)) {
    std::cerr << __LINE__ << " Failed to close file" << std::endl;
    std::abort();
  }
  const auto elapsed_time = util::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

#endif //METALL_FREE_MEMORY_SPACE_HPP
