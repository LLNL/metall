// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <algorithm>
#include <string>
#include <cassert>
#include <iomanip>
#include <type_traits>
#include <thread>
#include <vector>

#include <metall/detail/time.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/file.hpp>

namespace mdtl = metall::mtlldetail;

static constexpr int k_map_nosync =
#ifdef MAP_NOSYNC
    MAP_NOSYNC;
#else
    0;
#warning "MAP_NOSYNC is not defined"
#endif

template <typename random_iterator_type>
void init_array(random_iterator_type first, random_iterator_type last) {
  const std::size_t length = std::abs(std::distance(first, last));
  const auto num_threads = (int)std::min(
      (std::size_t)length, (std::size_t)std::thread::hardware_concurrency());
  std::vector<std::thread *> threads(num_threads, nullptr);

  const auto start = mdtl::elapsed_time_sec();
  for (int t = 0; t < num_threads; ++t) {
    const auto range = mdtl::partial_range(length, t, num_threads);
    threads[t] = new std::thread(
        [](random_iterator_type partial_first,
           random_iterator_type partial_last) {
          for (; partial_first != partial_last; ++partial_first) {
            *partial_first = std::distance(partial_first, partial_last) - 1;
          }
        },
        first + range.first, first + range.second);
  }
  for (auto &th : threads) {
    th->join();
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

template <typename random_iterator_type>
void run_sort(random_iterator_type first, random_iterator_type last) {
  const std::size_t length = std::abs(std::distance(first, last));
  const auto num_threads = (int)std::min(
      (std::size_t)length, (std::size_t)std::thread::hardware_concurrency());
  std::vector<std::thread *> threads(num_threads, nullptr);

  const auto start = mdtl::elapsed_time_sec();
  for (int t = 0; t < num_threads; ++t) {
    const auto range = mdtl::partial_range(length, t, num_threads);
    threads[t] = new std::thread(
        [](random_iterator_type partial_first,
           random_iterator_type partial_last) {
          std::sort(partial_first, partial_last);
        },
        first + range.first, first + range.second);
  }
  for (auto &th : threads) {
    th->join();
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void sync_region(void *const address, const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();
  mdtl::os_msync(address, size, true);
  const auto elapsed_time = mdtl::elapsed_time_sec(start);

  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

template <typename random_iterator_type>
void validate_array(random_iterator_type first, random_iterator_type last) {
  const std::size_t length = std::abs(std::distance(first, last));
  const auto num_threads = (int)std::min(
      (std::size_t)length, (std::size_t)std::thread::hardware_concurrency());

  const auto start = mdtl::elapsed_time_sec();
  for (int t = 0; t < num_threads; ++t) {
    const auto range = mdtl::partial_range(length, t, num_threads);

    if (range.second - range.first == 1) continue;

    for (auto itr = first + range.first; itr != (first + range.second - 2);
         ++itr) {
      if (*itr > *(itr + 1)) {
        std::cerr << __LINE__ << " Sort result is not correct: " << *itr
                  << " > " << *(itr + 1) << std::endl;
        std::abort();
      }
    }
  }
  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

void *map_with_single_file(const std::string &file_prefix,
                           const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();

  std::cout << "size: " << size << std::endl;

  const std::string file_name(file_prefix + "_single");
  if (!mdtl::create_file(file_name) ||
      !mdtl::extend_file_size(file_name, size)) {
    std::cerr << __LINE__ << " Failed to initialize file: " << file_name
              << std::endl;
    std::abort();
  }

  const auto ret =
      mdtl::map_file_write_mode(file_name, nullptr, size, 0, k_map_nosync);
  if (ret.first == -1 || !ret.second) {
    std::cerr << __LINE__ << " Failed mapping" << std::endl;
    std::abort();
  }
  if (!mdtl::os_close(ret.first)) {
    std::cerr << __LINE__ << " Failed to close file: " << file_name
              << std::endl;
    std::abort();
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return ret.second;
}

void *map_with_multiple_files(const std::string &file_prefix,
                              const std::size_t size,
                              const std::size_t chunk_size) {
  assert(size % chunk_size == 0);
  const auto start = mdtl::elapsed_time_sec();

  char *addr = reinterpret_cast<char *>(mdtl::reserve_vm_region(size));
  if (!addr) {
    std::cerr << "Failed to reserve VM region" << std::endl;
    std::abort();
  }

  const std::size_t num_files = size / chunk_size;
  std::cout << "size: " << size << "\nchunk_size: " << chunk_size
            << "\nnum_files: " << num_files << std::endl;

  for (std::size_t i = 0; i < num_files; ++i) {
    const std::string file_name(file_prefix + "_1_" + std::to_string(i));
    if (!mdtl::create_file(file_name) ||
        !mdtl::extend_file_size(file_name, chunk_size)) {
      std::cerr << __LINE__ << " Failed to initialize file: " << file_name
                << std::endl;
      std::abort();
    }

    const auto ret = mdtl::map_file_write_mode(
        file_name, reinterpret_cast<void *>(addr + chunk_size * i), chunk_size,
        0, k_map_nosync | MAP_FIXED);
    if (ret.first == -1 || !ret.second) {
      std::cerr << __LINE__ << " Failed mapping" << std::endl;
      std::abort();
    }
    if (!mdtl::os_close(ret.first)) {
      std::cerr << __LINE__ << " Failed to close file: " << file_name
                << std::endl;
      std::abort();
    }
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return addr;
}

void *map_with_multiple_files_round_robin(const std::string &file_prefix,
                                          const std::size_t size,
                                          const std::size_t file_size,
                                          const std::size_t chunk_size) {
  assert(size % file_size == 0);
  const std::size_t num_files = size / file_size;
  assert(size % num_files == 0);

  const auto start = mdtl::elapsed_time_sec();

  char *addr = reinterpret_cast<char *>(mdtl::reserve_vm_region(size));
  if (!addr) {
    std::cerr << "Failed to reserve VM region" << std::endl;
    std::abort();
  }

  const std::size_t num_chunks = size / chunk_size;
  const std::size_t num_chunks_per_file = num_chunks / num_files;

  std::cout << "size: " << size << "\nfile_size: " << file_size
            << "\nchunk_size: " << chunk_size << "\nnum_files: " << num_files
            << "\nnum chunks: " << num_chunks
            << "\nnum_chunks_per_file: " << num_chunks_per_file << std::endl;
  for (std::size_t chunk_no = 0; chunk_no < num_chunks; ++chunk_no) {
    const std::size_t round_no = chunk_no / num_files;
    const std::size_t file_no = chunk_no % num_files;

    const std::string file_name(file_prefix + "_2_" + std::to_string(file_no));
    if (round_no == 0) {  // first chunk of each file
      if (!mdtl::create_file(file_name) ||
          !mdtl::extend_file_size(file_name,
                                  chunk_size * num_chunks_per_file)) {
        std::cerr << __LINE__ << " Failed to initialize file: " << file_name
                  << std::endl;
        std::abort();
      }
    }

    const auto ret = mdtl::map_file_write_mode(
        file_name, reinterpret_cast<void *>(addr + chunk_size * chunk_no),
        chunk_size, chunk_size * round_no, k_map_nosync | MAP_FIXED);
    if (ret.first == -1 || !ret.second) {
      std::cerr << __LINE__ << " Failed mapping" << std::endl;
      std::abort();
    }
    if (!mdtl::os_close(ret.first)) {
      std::cerr << __LINE__ << " Failed to close file: " << file_name
                << std::endl;
      std::abort();
    }
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;

  return addr;
}

void unmap(void *const addr, const std::size_t size) {
  const auto start = mdtl::elapsed_time_sec();

  if (!mdtl::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }

  const auto elapsed_time = mdtl::elapsed_time_sec(start);
  std::cout << __FUNCTION__ << " took\t" << elapsed_time << std::endl;
}

template <typename T>
void run_bench(T *array, const std::size_t size) {
  init_array(array, array + size / sizeof(uint64_t));
  run_sort(array, array + size / sizeof(uint64_t));
  sync_region(array, size);
  validate_array(array, array + size / sizeof(uint64_t));
  unmap(array, size);
}

// ./a.out [mode] [file prefix] [total size] [each_file_size]
// [round_robin_chunk_size]
int main([[maybe_unused]] int argc, char *argv[]) {
  const int mode = std::stoul(argv[1]);

  const std::string file_prefix(argv[2]);
  assert(!file_prefix.empty());

  const std::size_t size = std::stoll(argv[3]);
  assert(size > sizeof(uint64_t));
  assert(size % sizeof(uint64_t) == 0);

  std::size_t each_file_size{0};
  if (mode == 1 || mode == 2) {
    each_file_size = std::stoll(argv[4]);
    assert(size % each_file_size == 0);
  }

  std::size_t round_robin_chunk_size{0};
  if (mode == 2) {
    round_robin_chunk_size = std::stoll(argv[5]);
    assert(size % round_robin_chunk_size == 0);
  }

  std::cout << std::fixed;
  std::cout << std::setprecision(2);

  if (mode == 0) {
    std::cout << "\nSingle file" << std::endl;
    auto array =
        reinterpret_cast<uint64_t *>(map_with_single_file(file_prefix, size));
    run_bench(array, size);
  } else if (mode == 1) {
    std::cout << "\nMany files (partition)" << std::endl;

    auto array = reinterpret_cast<uint64_t *>(
        map_with_multiple_files(file_prefix, size, each_file_size));
    run_bench(array, size);

  } else if (mode == 2) {
    std::cout << "\nMany files (round robin)" << std::endl;

    auto array =
        reinterpret_cast<uint64_t *>(map_with_multiple_files_round_robin(
            file_prefix, size, each_file_size, round_robin_chunk_size));
    run_bench(array, size);
  }

  return 0;
}