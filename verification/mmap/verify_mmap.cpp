// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// g++ -std=c++11 -pthread -O3 -o verify_mmap ./verify_mmap.cpp
// ./verify_mmap /l/ssd/file $((2**30))

#include <unistd.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <iostream>
#include <algorithm>
#include <string>
#include <cassert>
#include <type_traits>
#include <thread>
#include <random>
#include <vector>
#include <mutex>
#include <chrono>

inline std::chrono::high_resolution_clock::time_point elapsed_time_sec() {
  return std::chrono::high_resolution_clock::now();
}

inline double elapsed_time_sec(
    const std::chrono::high_resolution_clock::time_point &tic) {
  auto duration_time = std::chrono::high_resolution_clock::now() - tic;
  return static_cast<double>(
      std::chrono::duration_cast<std::chrono::microseconds>(duration_time)
          .count() /
      1e6);
}

void remove_file(const std::string &file_name) {
  std::cout << "Remove " << file_name << std::endl;
  std::string rm_command("rm -rf " + file_name);
  std::system(rm_command.c_str());
  std::cout << __FUNCTION__ << " done" << std::endl;
}

std::size_t get_system_page_size() {
  const ssize_t page_size = ::sysconf(_SC_PAGE_SIZE);
  if (page_size == -1) {
    ::perror("sysconf(_SC_PAGE_SIZE)");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  return static_cast<std::size_t>(page_size);
}

void os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    ::perror("fsync");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
}

void *map_file_read_mode(const std::string &file_name, const std::size_t size) {
  std::cout << "Map file (read mode): " << file_name << std::endl;
  std::cout << "Map size: " << size << std::endl;

  // Open a file
  const int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  os_fsync(fd);

  void *mapped_addr = ::mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
  ::close(fd);  // It is ok to close descriptor after mapping
  if (mapped_addr == nullptr) {
    ::perror("mmap");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  std::cout << "Mapped to address " << (uint64_t)mapped_addr << std::endl;

  std::cout << __FUNCTION__ << " done" << std::endl;

  return mapped_addr;
}

void *map_file_write_mode(const std::string &file_name,
                          const std::size_t size) {
  std::cout << "Map file (write mode): " << file_name << std::endl;
  std::cout << "Map size: " << size << std::endl;

  // Create a file
  const int fd =
      ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  os_fsync(fd);

  // Extent the file
  // This command makes a sparse file, i.e.,
  // physical memory space won't be used until the corresponding pages are
  // accessed
  if (::ftruncate(fd, size) == -1) {
    ::perror("ftruncate");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }

  // Map the file with read and write mode
  void *mapped_addr =
      ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  ::close(fd);  // It is ok to close descriptor after mapping
  if (mapped_addr == nullptr) {
    ::perror("mmap");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  std::cout << "Mapped to address " << (uint64_t)mapped_addr << std::endl;

  std::cout << __FUNCTION__ << " done" << std::endl;

  return mapped_addr;
}

void unmap(void *const addr, const std::size_t size) {
  std::cout << "Unmap file: address " << (uint64_t)addr << ", size " << size
            << std::endl;
  if (::munmap(addr, size) != 0) {
    ::perror("munmap");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  std::cout << __FUNCTION__ << " done" << std::endl;
}

std::vector<std::size_t> gen_random_index(const std::size_t max_index,
                                          const std::size_t num_indices) {
  std::vector<std::size_t> index_list(num_indices);

  std::random_device rd;
  std::mt19937_64 g(rd());
  std::uniform_int_distribution<std::size_t> dist(0, max_index);

  for (auto &elem : index_list) {
    elem = dist(g);
  }

  return index_list;
}

void write_data_with_single_thread(const std::string &file_name,
                                   const std::size_t file_size) {
  std::cout << "\nWrite data with a single thread" << std::endl;
  remove_file(file_name);

  auto const buf =
      static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
  const std::size_t length = file_size / sizeof(uint64_t);

  std::cout << "Write data" << std::endl;
  const auto start = elapsed_time_sec();
  for (uint64_t i = 0; i < length; ++i) {
    buf[i] = i;
  }

  for (uint64_t i = 0; i < length; ++i) {
    buf[i] *= 2;
  }
  std::cout << "Writing data took (sec.)\t" << elapsed_time_sec(start)
            << std::endl;

  unmap(buf, file_size);
}

void validate_data_with_single_thread(const std::string &file_name,
                                      const std::size_t file_size) {
  std::cout << "Validate data with a single thread" << std::endl;
  remove_file(file_name);

  auto const buf =
      static_cast<uint64_t *>(map_file_read_mode(file_name, file_size));
  const std::size_t length = file_size / sizeof(uint64_t);

  std::cout << "Validate data" << std::endl;
  for (uint64_t i = 0; i < length; ++i) {
    if (buf[i] != i * 2) {
      std::cerr << "Failed validation at " << i << std::endl;
      std::abort();
    }
  }

  std::cout << "Validating data done" << std::endl;

  unmap(buf, file_size);
}

void write_data_with_multiple_threads(const std::string &file_name,
                                      const std::size_t file_size) {
  std::cout << "\nWrite data with multiple threads" << std::endl;
  remove_file(file_name);

  auto const buf =
      static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
  const std::size_t length = file_size / sizeof(uint64_t);

  const auto num_threads = (int)std::min(
      (std::size_t)length, (std::size_t)std::thread::hardware_concurrency());
  std::cout << "#of threads: " << num_threads << std::endl;

  std::cout << "Generate index" << std::endl;
  std::vector<std::vector<std::size_t>> index_list(num_threads);
  {
    std::vector<std::thread *> threads(num_threads, nullptr);
    const std::size_t num_indices = (length + num_threads - 1) / num_threads;
    for (int t = 0; t < num_threads; ++t) {
      threads[t] = new std::thread(
          [&length, &num_indices, &index_list](const int thread_no) {
            index_list[thread_no] = gen_random_index(length - 1, num_indices);
          },
          t);
    }
    for (auto &th : threads) {
      th->join();
    }

    for (const auto &indices : index_list) {
      for (const auto &index : indices) {
        assert(index < length);
      }
    }
  }

  std::cout << "Write data" << std::endl;
  {
    std::vector<std::mutex> mutex_list(std::thread::hardware_concurrency() *
                                       128);
    std::vector<std::thread *> threads(num_threads, nullptr);

    const auto start = elapsed_time_sec();
    for (int t = 0; t < num_threads; ++t) {
      threads[t] = new std::thread(
          [&index_list, &mutex_list, buf](const int thread_no) {
            for (const auto idx : index_list[thread_no]) {
              std::lock_guard<std::mutex> guard(
                  mutex_list[idx % mutex_list.size()]);
              buf[idx] = idx;
            }
          },
          t);
    }
    for (auto &th : threads) {
      th->join();
    }
    std::cout << "Writing data took (sec.)\t" << elapsed_time_sec(start)
              << std::endl;
  }

  unmap(buf, file_size);
}

// a.out file_name file_size
int main(int argc, char *argv[]) {
  const std::string file_name(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);

  write_data_with_single_thread(file_name, file_size);

  write_data_with_multiple_threads(file_name, file_size);

  return 0;
}