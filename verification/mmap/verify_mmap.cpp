// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <algorithm>
#include <string>
#include <cassert>
#include <type_traits>
#include <thread>
#include <random>
#include <vector>
#include <mutex>

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

std::size_t get_system_page_size() {
  const auto page_size = util::get_page_size();
  if (page_size <= 0) {
    std::cerr << "Failed to get page size" << std::endl;
    std::abort();
  }
  std::cout << "Page size: " << page_size << std::endl;

  return static_cast<std::size_t>(page_size);
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

  std::cout << "Mapped to " << (uint64_t)ret.second << std::endl;

  std::cout << __FUNCTION__ << " done" << std::endl;

  return ret.second;
}

void unmap(void *const addr, const std::size_t size) {
  std::cout << "Unmap file: " << (uint64_t)addr << " " << size << std::endl;
  if (!util::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }
  std::cout << __FUNCTION__ << " done" << std::endl;
}

auto gen_random_index(const std::size_t length) {

  std::vector<std::size_t> index_list(length);

  std::generate(index_list.begin(), index_list.end(), [n = (std::size_t)0]() mutable { return n++; });

  std::random_device rd;
  std::mt19937_64 g(rd());
  std::shuffle(index_list.begin(), index_list.end(), g);

  for (const auto idx : index_list) {
    assert(idx < length);
  }

  return index_list;
}

// a.out file_name file_size
int main(int argc, char *argv[]) {

  std::string file_name(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);

  std::cout << "File: " << file_name << std::endl;

  {
    std::cout << "Single thread mmap verification" << std::endl;
    remove_file(file_name);

    auto const buf = static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
    const std::size_t length = file_size / sizeof(uint64_t);

    std::cout << "Write data" << std::endl;
    for (uint64_t i = 0; i < length; ++i) {
      buf[i] = i;
    }

    for (uint64_t i = 0; i < length; ++i) {
      buf[i] *= 2;
    }

    std::cout << "Write data done" << std::endl;

    unmap(buf, file_size);
  }

  {
    std::cout << "Multi-thread mmap verification" << std::endl;
    remove_file(file_name);

    auto const buf = static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
    const std::size_t length = file_size / sizeof(uint64_t);

    const auto num_threads = (int)std::min((std::size_t)length, (std::size_t)std::thread::hardware_concurrency());
    std::cout << "#of threads: " << num_threads << std::endl;

    const int num_mutex = 1024;
    std::mutex mutex_list[num_mutex];

    std::cout << "Generate index" << std::endl;
    std::vector<std::vector<std::size_t>> index_list(num_threads);
    {
      std::vector<std::thread *> threads(num_threads, nullptr);
      for (int t = 0; t < num_threads; ++t) {
        threads[t] = new std::thread([&length, &num_threads, &index_list](const int thread_no) {
                                       index_list[thread_no] = gen_random_index(length);
                                       index_list[thread_no].resize(length / num_threads);
                                     },
                                     t);
      }
      for (auto &th : threads) {
        th->join();
      }
    }

    std::cout << "Write data" << std::endl;
    {
      std::vector<std::thread *> threads(num_threads, nullptr);
      for (int t = 0; t < num_threads; ++t) {
        threads[t] = new std::thread([&index_list, &mutex_list, buf](const int thread_no) {
                                       for (const auto idx : index_list[thread_no]) {
                                         std::lock_guard<std::mutex> guard(mutex_list[idx % num_mutex]);
                                         buf[idx] = idx;
                                       }
                                     },
                                     t);
      }
      for (auto &th : threads) {
        th->join();
      }
    }
    std::cout << "Write data done" << std::endl;

    unmap(buf, file_size);
  }

  return 0;
}