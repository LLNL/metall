// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// A program to verify mmap with multi-thread
// This program is not included Metall's CMake build
// Build
//  g++ -fopenmp -o verify_mmap_openmp verify_mmap_openmp.cpp
// Run
//  ./verify_mmap_openmp file_name file_size

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include <iostream>
#include <string>
#include <cassert>
#include <random>

void *map_file_write_mode(const std::string &file_name,
                          const std::size_t size) {
  // Create a file
  const int fd =
      ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }

  // Extend the file size
  if (::ftruncate(fd, size) == -1) {
    ::perror("ftruncate");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }

  // Map the file
  void *mapped_addr =
      ::mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (mapped_addr == MAP_FAILED) {
    ::perror("mmap");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
  ::close(fd);

  return mapped_addr;
}

void unmap(void *const addr, const std::size_t size) {
  if (::munmap(addr, size) != 0) {
    ::perror("munmap");
    std::cerr << "errno: " << errno << std::endl;
    std::abort();
  }
}

int get_num_threads() noexcept {
#ifdef _OPENMP
  return ::omp_get_num_threads();
#else
  return 1;
#endif
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    std::cerr << "Wrong arguments: a.out [file name] [file size]" << std::endl;
    std::abort();
  }
  std::string file_name(argv[1]);
  const std::size_t file_size = std::stoll(argv[2]);

  std::cout << "\nMap a file: " << file_name << ", " << file_size << " bytes"
            << std::endl;
  auto const buf =
      static_cast<uint64_t *>(map_file_write_mode(file_name, file_size));
  const std::size_t length = file_size / sizeof(uint64_t);

  std::cout << "\nWrite data" << std::endl;
#ifdef _OPENMP
#pragma omp parallel
#endif
  {
#ifdef _OPENMP
#pragma omp single
#endif
    { std::cout << "#of threads: " << get_num_threads() << std::endl; }

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

  std::cout << "\nUnmap" << std::endl;
  unmap(buf, length);

  std::cout << "Succeeded!!" << std::endl;

  return 0;
}