// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Benchmarks file mapping backends.
/// Usage:
/// ./run_mapping_bench
/// # modify some values in the main function, if needed.

#include <iostream>
#include <random>
#include <string>
#include <map>
#include <vector>
#include <metall/metall.hpp>
#include <metall/utility/random.hpp>
#include <metall/detail/time.hpp>
#include <metall/detail/mmap.hpp>

using rand_engine = metall::utility::rand_512;
static constexpr std::size_t k_page_size = 4096;

namespace {
namespace mdtl = metall::mtlldetail;
}

auto random_write_by_page(const std::size_t size, unsigned char *const map) {
  const auto num_pages = size / k_page_size;
  rand_engine rand_engine(123);
  std::uniform_int_distribution<> dist(0, num_pages - 1);

  const auto s = mdtl::elapsed_time_sec();
  for (std::size_t i = 0; i < num_pages; ++i) {
    const auto page_no = dist(rand_engine);
    const off_t offset = static_cast<off_t>(page_no * k_page_size);
    map[offset] = '0';
  }
  const auto t = mdtl::elapsed_time_sec(s);

  return t;
}

auto random_read_by_page(const std::size_t size,
                         const unsigned char *const map) {
  const auto num_pages = size / k_page_size;
  rand_engine rand_engine(1234);
  std::uniform_int_distribution<> dist(0, num_pages - 1);

  const auto s = mdtl::elapsed_time_sec();
  for (std::size_t i = 0; i < num_pages; ++i) {
    const auto page_no = dist(rand_engine);
    const off_t offset = static_cast<off_t>(page_no * k_page_size);
    [[maybe_unused]] volatile char dummy = map[offset];
  }
  const auto t = mdtl::elapsed_time_sec(s);

  return t;
}

int create_normal_file(std::string_view path) {
  const int fd =
      ::open(path.data(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    std::cerr << "Failed to create a file" << std::endl;
    std::abort();
  }
  return fd;
}

int create_tmpfile(std::string_view path) {
  static char file_template[] = "/mmap.XXXXXX";

  char fullname[path.size() + sizeof(file_template)];
  (void)strcpy(fullname, path.data());
  (void)strcat(fullname, file_template);

  int fd = -1;
  if ((fd = mkstemp(fullname)) < 0) {
    std::perror("Could not create temporary file");
    std::abort();
  }

  (void)unlink(fullname);

  return fd;
}

void extend_file(const int fd, const std::size_t size,
                 const bool fill_with_zero) {
  if (!mdtl::extend_file_size(fd, size, fill_with_zero)) {
    std::cerr << "Failed to extend file" << std::endl;
    std::abort();
  }
}

auto map_file(const int fd, const std::size_t size) {
  static constexpr int k_map_nosync =
#ifdef MAP_NOSYNC
      MAP_NOSYNC;
#else
      0;
#warning "MAP_NOSYNC is not defined"
#endif

  auto *const map = mdtl::os_mmap(NULL, size, PROT_READ | PROT_WRITE,
                                  MAP_SHARED | k_map_nosync, fd, 0);
  if (!map) {
    std::cerr << " Failed mapping" << std::endl;
    std::abort();
  }

  return map;
}

void close_file(const int fd) {
  if (!mdtl::os_close(fd)) {
    std::cerr << __LINE__ << " Failed to close file" << std::endl;
    std::abort();
  }
}

void unmap(void *const addr, const std::size_t size) {
  if (!mdtl::munmap(addr, size, false)) {
    std::cerr << __LINE__ << " Failed to munmap" << std::endl;
    std::abort();
  }
}

/// \brief Run benchmark to evaluate different mapping methods
void run_bench_one_time(
    std::string_view dir_path, const std::size_t length,
    const bool init_file_writing_zero,
    std::map<std::string, std::vector<double>> &time_table) {
  const auto bench_core = [&time_table, length](std::string_view mode,
                                                unsigned char *const map) {
    time_table[std::string(mode) + " write"].push_back(
        random_write_by_page(length, map));
    time_table[std::string(mode) + " read"].push_back(
        random_read_by_page(length, map));
  };

  // Use 'new'
  {
    auto *const map = new unsigned char[length];
    bench_core("malloc", map);
    delete[] map;
  }

  // Use a normal file and mmap
  {
    std::string file_path{std::string(dir_path) + "/map-file"};
    const int fd = create_normal_file(file_path);
    extend_file(fd, length, init_file_writing_zero);
    auto *const map = static_cast<unsigned char *>(map_file(fd, length));
    close_file(fd);
    bench_core("Normal-file", map);
    unmap(map, length);
  }

  // Use tmpfile and mmap
  {
    const int fd = create_tmpfile(dir_path);
    extend_file(fd, length, init_file_writing_zero);
    auto *const map = static_cast<unsigned char *>(map_file(fd, length));
    close_file(fd);
    bench_core("tmpfile", map);
    unmap(map, length);
  }

  // Use Metall
  {
    metall::manager manager(metall::create_only, dir_path.data());
    auto *map = static_cast<unsigned char *>(manager.allocate(length));
    bench_core("Metall", map);
    manager.deallocate(map);
  }
  metall::manager::remove(dir_path.data());
}

void run_bench(std::string_view dir_path, const std::size_t num_repeats,
               const std::size_t length, const bool init_file_writing_zero) {
  std::cout << "\n----------" << std::endl;
  std::cout << "Directory Path:\t" << dir_path << "\nRepeats:\t" << num_repeats
            << "\nLength:\t" << length << "\nInit w/ writing:\t"
            << init_file_writing_zero << "\n"
            << std::endl;

  // Run bench
  std::map<std::string, std::vector<double>> time_table;
  for (std::size_t i = 0; i < num_repeats; ++i) {
    run_bench_one_time(dir_path, length, init_file_writing_zero, time_table);
  }

  // Show results
  for (const auto &entry : time_table) {
    const auto &mode = entry.first;
    const auto &times = entry.second;
    std::cout << std::fixed;
    std::cout << std::setprecision(2);
    std::cout << mode << " took (s)\t"
              << std::accumulate(times.begin(), times.end(), 0.0f) /
                     times.size()
              << std::endl;
  }
}

int main() {
  static constexpr std::size_t size = k_page_size * 1024 * 10;
  const int num_repeats = 10;

#if defined(__linux__)
  run_bench("/dev/shm", num_repeats, size, false);
  run_bench("/dev/shm", num_repeats, size, true);
#endif
  run_bench("/tmp", num_repeats, size, false);
  run_bench("/tmp", num_repeats, size, true);

  return 0;
}