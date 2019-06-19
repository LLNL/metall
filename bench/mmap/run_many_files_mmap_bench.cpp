// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <algorithm>
#include <string>
#include <cassert>
#include <iomanip>

#include "../utility/time.hpp"
#include "../../include/metall/detail/utility/mmap.hpp"
#include "../../include/metall/detail/utility/file.hpp"

namespace util = metall::detail::utility;

static constexpr int k_map_nosync =
#ifdef MAP_NOSYNC
    MAP_NOSYNC;
#else
    0;
#endif

template <typename random_iterator_type>
void run_sort(random_iterator_type first, random_iterator_type last) {
  const auto start = utility::elapsed_time_sec();

  std::sort(first, last);
  const auto elapsed_time = utility::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

template <typename random_iterator_type>
void init_array(random_iterator_type first, random_iterator_type last) {
  const auto start = utility::elapsed_time_sec();

  for (; first != last; ++first) {
    *first = std::distance(first, last) - 1;
  }
  const auto elapsed_time = utility::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

template <typename random_iterator_type>
void validate_array(random_iterator_type first, random_iterator_type last) {
  const auto start = utility::elapsed_time_sec();

  for (auto itr = first; itr != last; ++itr) {
    if (*itr != std::distance(first, itr)) {
      std::cerr << __LINE__ << " Sort result is not correct: "
                << *itr << " != " << std::distance(first, itr) << std::endl;
      std::abort();
    }
  }
  const auto elapsed_time = utility::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void *map_with_single_file(const std::string &file_prefix, const std::size_t size) {
  const auto start = utility::elapsed_time_sec();

  const std::string file_name(file_prefix + "_single");
  if (!util::create_file(file_name) || !util::extend_file_size(file_name, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_name << std::endl;
    std::abort();
  }

  const auto ret = util::map_file_write_mode(file_name, nullptr, size, 0, k_map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }

  const auto elapsed_time = utility::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return ret.second;
}

void *map_with_multiple_files(const std::string &file_prefix, const std::size_t size, const std::size_t chunk_size) {
  assert(size % chunk_size == 0);
  const auto start = utility::elapsed_time_sec();

  char *addr = reinterpret_cast<char *>(util::reserve_vm_region(size));
  if (!addr) {
    std::cerr << "Failed to reserve VM region" << std::endl;
    std::abort();
  }

  const std::size_t num_files = size / chunk_size;

  for (std::size_t i = 0; i < num_files; ++i) {
    const std::string file_name(file_prefix + "_" + std::to_string(i));
    if (!util::create_file(file_name) || !util::extend_file_size(file_name, chunk_size)) {
      std::cerr << __LINE__ << " Failed to initialize file: " << file_name << std::endl;
      std::abort();
    }

    const auto ret = util::map_file_write_mode(file_name,
                                               reinterpret_cast<void *>(addr + chunk_size * i),
                                               chunk_size,
                                               0,
                                               k_map_nosync | MAP_FIXED);
    if (ret.first == -1 || !ret.second) {
      std::cerr << __LINE__ << " Failed mapping" << std::endl;
      std::abort();
    }
  }

  const auto elapsed_time = utility::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return addr;
}

void unmap(void *addr, const std::size_t size) {
  const auto start = utility::elapsed_time_sec();

  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }

  const auto elapsed_time = utility::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

int main(int argc, char *argv[]) {

  if (argc != 4) {
    std::cerr << "Wrong arguments" << std::endl;
    std::abort();
  }
  std::cout << std::fixed;
  std::cout << std::setprecision(2);

  std::cout << "\nSingle file" << std::endl;
  const std::string file_prefix(argv[1]);
  const std::size_t length = std::stoll(argv[2]);
  const std::size_t chunk_length = std::stoll(argv[3]);
  {
    auto array = reinterpret_cast<uint64_t *>(map_with_single_file(file_prefix, length * sizeof(uint64_t)));
    init_array(array, array + length);
    run_sort(array, array + length);
    validate_array(array, array + length);
    unmap(array, length * sizeof(uint64_t));
  }

  std::cout << "\nMany files" << std::endl;
  {
    auto array = reinterpret_cast<uint64_t *>(map_with_multiple_files(file_prefix,
                                                                      length * sizeof(uint64_t),
                                                                      chunk_length * sizeof(uint64_t)));
    init_array(array, array + length);
    run_sort(array, array + length);
    validate_array(array, array + length);
    unmap(array, length * sizeof(uint64_t));
  }

  return 0;
}