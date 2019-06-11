// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY__FILE_HPP
#define METALL_DETAIL_UTILITY__FILE_HPP

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>
#include <fstream>

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <iostream>
#include <fstream>
#include <filesystem>

// TODO: change to C++17

namespace metall {
namespace detail {
namespace utility {

namespace {
namespace fs = std::filesystem;
}

void extend_file_size_manually(const int fd, const ssize_t file_size) {
  auto buffer = new unsigned char[4096];
  for (off_t i = 0; i < file_size / 4096; ++i) {
    ::pwrite(fd, buffer, 4096, i * 4096);
  }
  const size_t remained_size = file_size % 4096;
  if (remained_size > 0)
    ::pwrite(fd, buffer, remained_size, file_size - remained_size);

  ::sync();
  delete[] buffer;
}

bool extend_file_size(const int fd, const size_t file_size) {
  /// -----  extend the file if its size is smaller than that of mapped area ----- ///
  struct stat statbuf;
  if (::fstat(fd, &statbuf) == -1) {
    ::perror("fstat");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  if (::llabs(statbuf.st_size) < static_cast<ssize_t>(file_size)) {
    if (::ftruncate(fd, file_size) == -1) {
      ::perror("ftruncate");
      std::cerr << "errno: " << errno << std::endl;
      return false;
    }
  }
  return true;
}

bool extend_file_size(const std::string &file_name, const size_t file_size) {
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  const bool ret = extend_file_size(fd, file_size);

  ::close(fd);

  return ret;
}

bool create_file(const std::string &file_name) {
  const int fd = ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  ::close(fd);

  return true;
}

ssize_t get_file_size(const std::string &file_name) {
  std::ifstream ifs(file_name, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    std::cerr << "Failed to get file size: " << file_name << std::endl;
  }

  return size;
}

ssize_t get_actual_file_size(const std::string &file_name) {
  struct stat statbuf;
  if (::stat(file_name.c_str(), &statbuf) != 0) {
    ::perror("stat");
    std::cerr << "errno: " << errno << std::endl;
    return -1;
  }
  return statbuf.st_blocks * 512LL;
}

bool remove_file(const std::string &file_name) {
  return (std::remove(file_name.c_str()) == 0);
}

bool file_exist(const std::string &file_name) {
  struct stat statbuf;
  return (::stat(file_name.c_str(), &statbuf) == 0);
}

#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
void deallocate_file_space(const int fd, const off_t off, const off_t len) {
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len) == -1) {
    ::perror("fallocate");
    std::abort();
  }
}
#else
#warning "FALLOC_FL_PUNCH_HOLE or FALLOC_FL_KEEP_SIZE is not supported"
void deallocate_file_space([[maybe_unused]] const int fd, [[maybe_unused]] const off_t off, [[maybe_unused]] const off_t len) {
}
#endif

bool copy_file(const std::string &source_path, const std::string &destination_path) {

  bool success = true;

  try {
    if (!fs::copy_file(source_path, destination_path, fs::copy_options::overwrite_existing)) {
      std::cerr << "Failed copying file: " << source_path << " -> " << destination_path << std::endl;
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    std::cerr << e.what() << std::endl;
    success = false;
  }

  return success;
}

bool os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    ::perror("fsync");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY__FILE_HPP
