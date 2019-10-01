// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include <iostream>
#include <string>
#include <cassert>
#include <vector>
#include <random>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/file.hpp>

namespace util = metall::detail::utility;

void remove_file(const std::string &file_name) {
  if (!util::remove_file(file_name)) {
    std::cerr << "Failed to remove file: " << file_name << std::endl;
    std::abort();
  }
  std::cout << __FUNCTION__ << " done" << std::endl;
}

void *map_file_write_mode(const std::string &file_name, const std::size_t size) {
  std::cout << "Map file: " << file_name << std::endl;
  std::cout << "Map size: " << size << std::endl;

  if (!util::create_file(file_name) || !util::extend_file_size(file_name, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_name << std::endl;
    std::abort();
  }

  const auto ret = util::map_file_write_mode(file_name, nullptr, size, 0);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }
  ::close(ret.first);

  std::cout << "Mapped address: " << (uint64_t)ret.second << std::endl;

  std::cout << __FUNCTION__ << " done" << std::endl;

  return ret.second;
}

void unmap(void *const addr, const std::size_t size) {
  std::cout << "Unmap address: " << (uint64_t)addr << std::endl;
  std::cout << "Unmap size: " << size << std::endl;

  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }
  std::cout << __FUNCTION__ << " done" << std::endl;
}

// a.out file_name file_size
int main(int argc, char *argv[]) {

  if (argc != 3) {
    std::cerr << "Wrong arguments: a.out [file name] [file size]" << std::endl;
    std::abort();
  }
  std::string file_name(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);

  remove_file(file_name);

  auto const buf = static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
  const std::size_t length = file_size / sizeof(uint64_t);

  std::cout << "Write data" << std::endl;
#ifdef _OPENMP
#pragma omp parallel
#endif
  {
    std::random_device rd;
    std::mt19937_64 g(rd());
    for (std::size_t i = 0; i < length; ++i) {
      auto dis = std::uniform_int_distribution<std::size_t>(0, length - 1);
      const auto idx = dis(g);
#ifdef _OPENMP
#pragma omp atomic
#endif
      ++(buf[idx]);
    }
  }
  std::cout << "Write data succeeded" << std::endl;

  unmap(buf, length);

  return 0;
}