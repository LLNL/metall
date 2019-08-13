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

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <iostream>
#include <fstream>
#ifdef METALL_FOUND_CPP17_FILESYSTEM_LIB
#include <filesystem>
#endif

namespace metall {
namespace detail {
namespace utility {

#ifdef METALL_FOUND_CPP17_FILESYSTEM_LIB
namespace {
namespace fs = std::filesystem;
}
#endif

inline void extend_file_size_manually(const int fd, const ssize_t file_size) {
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

inline bool extend_file_size(const int fd, const size_t file_size) {
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

inline bool extend_file_size(const std::string &file_name, const size_t file_size) {
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

inline bool create_file(const std::string &file_name) {
  const int fd = ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  ::close(fd);

  return true;
}

#ifdef METALL_FOUND_CPP17_FILESYSTEM_LIB
inline bool create_dir(const std::string &dir_path) {
  bool success = true;
  try {
    if (!fs::create_directories(dir_path)) {
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    // std::cerr << e.what() << std::endl;
    success = false;
  }
  return success;
}
#else
inline bool create_dir(const std::string &dir_path) {
  if (::mkdir(dir_path.c_str(), S_IRUSR | S_IWUSR | S_IXUSR) == -1) {
    return false;
  }
  return true;
}
#endif

inline ssize_t get_file_size(const std::string &file_name) {
  std::ifstream ifs(file_name, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    std::cerr << "Failed to get file size: " << file_name << std::endl;
  }

  return size;
}

/// \brief
/// Note that, according to GCC,
/// the file system may use some blocks for internal record keeping
inline ssize_t get_actual_file_size(const std::string &file_name) {
  struct stat statbuf;
  if (::stat(file_name.c_str(), &statbuf) != 0) {
    ::perror("stat");
    std::cerr << "errno: " << errno << std::endl;
    return -1;
  }
  return statbuf.st_blocks * 512LL;
}

inline bool remove_file(const std::string &file_name) {
  return (std::remove(file_name.c_str()) == 0);
}

/// \brief Check if a file or directory exist
inline bool file_exist(const std::string &file_name) {
  struct stat statbuf;
  return (::stat(file_name.c_str(), &statbuf) == 0);
}

inline bool free_file_space([[maybe_unused]] const int fd,
                            [[maybe_unused]] const off_t off,
                            [[maybe_unused]] const off_t len) {
#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len) == -1) {
    // TODO: use a logger to record this warning
    // ::perror("fallocate");
    // std::abort();
    return false;
  }
  return true;

#else
#warning "FALLOC_FL_PUNCH_HOLE or FALLOC_FL_KEEP_SIZE is not supported"
  return false;
#endif
}


#ifdef METALL_FOUND_CPP17_FILESYSTEM_LIB
inline bool copy_file(const std::string &source_path, const std::string &destination_path) {
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

#else

inline bool copy_file(const std::string &source_path, const std::string &destination_path) {
  {
    const ssize_t source_file_size = get_file_size(source_path);
    const ssize_t actual_source_file_size = get_actual_file_size(source_path);
    if (source_file_size == -1 || actual_source_file_size == -1) {
      return false;
    }

    // If the source file is empty, just create the destination file and done.
    if (source_file_size == 0 || actual_source_file_size == 0) {
      create_file(destination_path);
      return true;
    }
  }

  {
    std::ifstream source(source_path);
    if (!source.is_open()) {
      std::cerr << "Cannot open: " << source_path << std::endl;
      return false;
    }

    std::ofstream destination(destination_path);
    if (!destination.is_open()) {
      std::cerr << "Cannot open: " << destination_path << std::endl;
      return false;
    }

    destination << source.rdbuf();
    if (!destination) {
      std::cerr << "Something happened in the ofstream: " << destination_path << std::endl;
      return false;
    }

    destination.close();
  }

  {
    // Sanity check
    const ssize_t s1 = get_file_size(source_path);
    const ssize_t s2 = get_file_size(destination_path);
    if (s1 < 0 || s1 != s2) {
      std::cerr << "Something wrong in file sizes: " << s1 << " " << s2 << std::endl;
      return false;
    }
  }
  return true;
}

#endif

inline bool os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    ::perror("fsync");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}

inline bool fsync(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  const bool ret = os_fsync(fd);

  ::close(fd);

  return ret;
}

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY__FILE_HPP
