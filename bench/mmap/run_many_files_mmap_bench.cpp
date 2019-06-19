// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <algorithm>
#include <execution>
#include <string>
#include <cassert>

#include "../utility/time.hpp"
#include "../../include/metall/detail/utility/mmap.hpp"
#include "../../include/metall/detail/utility/file.hpp"

namespace util = metall::detail::utility;

template <typename random_iterator_type>
void run_sort(random_iterator_type first, random_iterator_type last) {
  const auto start = utility::elapsed_time_sec();
  std::sort(std::execution::parallel_policy(), first, last);
  const auto elapsed_time = utility::elapsed_time_sec(start);
  std::cout << __FILE__ << " took " << elapsed_time << std::endl;
}

template <typename random_iterator_type>
void init_array(random_iterator_type first, random_iterator_type last) {
  const auto start = utility::elapsed_time_sec();
  for (; first != last; ++first) {
    *first = std::distance(first, last);
  }
  const auto elapsed_time = utility::elapsed_time_sec(start);
  std::cout << __FILE__ << " took " << elapsed_time << std::endl;
}

void *map_with_single_file(const std::string &file_prefix, const std::size_t size) {
  const std::string file_name(file_prefix + "_single");
  if (!util::create_file(file_name) || !util::extend_file_size(file_prefix, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_name << std::endl;
    std::abort();
  }

  static constexpr int map_nosync =
#ifdef MAP_NOSYNC
      MAP_NOSYNC;
#else
      0;
#endif

  const auto ret = util::map_file_write_mode(file_name, nullptr, size, 0, map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }
  return ret.second;
}

void *map_with_multiple_files(const std::string &file_prefix, const std::size_t size, const std::size_t chunk_size) {
  assert(size % chunk_size == 0);

  char *addr = reinterpret_cast<char *>(util::reserve_vm_region(size));
  if (!addr) {
    std::cerr << "Failed to reserve VM region" << std::endl;
    std::abort();
  }

  const std::size_t num_files = size / chunk_size;

  for (std::size_t i = 0; i < num_files; ++i) {
    const std::string file_name(file_prefix + "_" + std::to_string(i));
    if (!util::create_file(file_name) || !util::extend_file_size(file_prefix, chunk_size)) {
      std::cerr << __LINE__ << " Failed to initialize file: " << file_name << std::endl;
      std::abort();
    }

    static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif

    const auto ret = util::map_file_write_mode(file_name,
                                               reinterpret_cast<void *>(addr + chunk_size * i),
                                               chunk_size,
                                               0,
                                               map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::cerr << __LINE__ << " Failed mapping" << std::endl;
      std::abort();
    }
    return ret.second;
  }
}

void unmap(void *addr, const std::size_t size) {
  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }
}

int main(int argc, char *argv[]) {

  if (argc == 4) {
    std::cerr << "Wrong arguments" << std::endl;
    std::abort();
  }

  const std::string file_prefix(argv[1]);
  const std::size_t length = std::stoll(argv[2]);
  const std::size_t chunk_size = std::stoll(argv[3]);
  {
    auto array = reinterpret_cast<uint64_t *>(map_with_single_file(file_prefix, length * sizeof(uint64_t)));
    init_array(array, array + length);
    run_sort(array, array + length);
  }

  {
    auto array = reinterpret_cast<uint64_t *>(map_with_multiple_files(file_prefix,
                                                                      length * sizeof(uint64_t),
                                                                      chunk_size));
    init_array(array, array + length);
    run_sort(array, array + length);
    unmap(array, length * sizeof(uint64_t));
  }

  return 0;
}