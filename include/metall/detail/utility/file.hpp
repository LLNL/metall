// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_FILE_HPP
#define METALL_DETAIL_UTILITY_FILE_HPP

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>

#ifdef __has_include
// Check if the Filesystem library is available
// METALL_NOT_USE_CXX17_FILESYSTEM_LIB could be set by the CMake configuration file
#if __has_include(<filesystem>) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
#include <filesystem>
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "The Filesystem library is not available."
#endif
#endif

#else // __has_include is not defined

#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "__has_include is not defined, consequently disable the Filesystem library."
#endif // METALL_VERBOSE_SYSTEM_SUPPORT_WARNING

#endif // #ifdef __has_include

#include <metall/logger.hpp>

namespace metall {
namespace detail {
namespace utility {

#if defined(__cpp_lib_filesystem) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
namespace {
namespace fs = std::filesystem;
}
#endif

inline bool os_close(const int fd) {
  if (::close(fd) == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "close");
    return false;
  }
  return true;
}

inline bool os_fsync(const int fd) {
  if (::fsync(fd) != 0) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "fsync");
    return false;
  }
  return true;
}

inline bool fsync(const std::string &path) {
  const int fd = ::open(path.c_str(), O_RDONLY);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return false;
  }

  bool ret = true;
  ret &= os_fsync(fd);
  ret &= os_close(fd);

  return ret;
}

inline bool fsync_recursive(const std::string &path) {
#if defined(__cpp_lib_filesystem) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
  fs::path p(path);
  p = fs::canonical(p);
  while (true) {
    if (!fsync(p.string())) {
      return false;
    }
    if (p == p.root_path()) {
      break;
    }
    p = p.parent_path();
  }
  return true;
#else
// FIXME: Implement recersive fsync w/o the C++17 Filesystem library
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "Cannot call fsync recursively"
#endif // METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
  return fsync(path);
#endif
}

inline bool extend_file_size_manually(const int fd, const off_t offset, const ssize_t file_size) {
  auto buffer = new unsigned char[4096];
  for (off_t i = offset; i < file_size / 4096 + offset; ++i) {
    ::pwrite(fd, buffer, 4096, i * 4096);
  }
  const size_t remained_size = file_size % 4096;
  if (remained_size > 0)
    ::pwrite(fd, buffer, remained_size, file_size - remained_size);

  delete[] buffer;

  const bool ret = os_fsync(fd);

  return ret;
}

inline bool extend_file_size(const int fd, const size_t file_size, const bool fill_with_zero) {

  if (fill_with_zero) {
#ifdef __APPLE__
    if (!extend_file_size_manually(fd, 0, file_size)) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to extend file size manually, filling zero");
      return false;
    }
#else
    if (::posix_fallocate(fd, 0, file_size) == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "fallocate");
      return false;
    }
#endif
  } else {
    // -----  extend the file if its size is smaller than that of mapped area ----- //
    struct stat statbuf;
    if (::fstat(fd, &statbuf) == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "fstat");
      return false;
    }
    if (::llabs(statbuf.st_size) < static_cast<ssize_t>(file_size)) {
      if (::ftruncate(fd, file_size) == -1) {
        logger::perror(logger::level::error, __FILE__, __LINE__, "ftruncate");
        return false;
      }
    }
  }

  const bool ret = os_fsync(fd);
  return ret;
}

inline bool extend_file_size(const std::string &file_name, const size_t file_size, const bool fill_with_zero = false) {
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return false;
  }

  bool ret = extend_file_size(fd, file_size, fill_with_zero);
  ret &= os_close(fd);

  return ret;
}

inline bool create_file(const std::string &file_name) {

  const int fd = ::open(file_name.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return false;
  }

  if (!os_close(fd))
    return false;

  return fsync_recursive(file_name);
}

#if defined(__cpp_lib_filesystem) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
inline bool create_directory(const std::string &dir_path) {
  bool success = true;
  try {
    std::error_code ec;
    if (!fs::create_directories(dir_path, ec)) {
      logger::out(logger::level::error, __FILE__, __LINE__, ec.message());
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    success = false;
  }
  return success;
}
#else
inline bool create_directory(const std::string &dir_path) {
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
    std::stringstream ss;
    ss << "Failed to get file size: " << file_name;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
  }

  return size;
}

/// \brief
/// Note that, according to GCC,
/// the file system may use some blocks for internal record keeping
inline ssize_t get_actual_file_size(const std::string &file_name) {
  struct stat statbuf;
  if (::stat(file_name.c_str(), &statbuf) != 0) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "stat (" + file_name + ")");
    return -1;
  }
  return statbuf.st_blocks * 512LL;
}

/// \brief Check if a file, any kinds of file including directory, exists
inline bool file_exist(const std::string &file_name) {
  return (::access(file_name.c_str(), F_OK) == 0);
}

/// \brief Check if a directory exists
inline bool directory_exist(const std::string &dir_path) {
  struct stat statbuf;
  if (::stat(dir_path.c_str(), &statbuf) == -1) {
    return false;
  }
  return (uint64_t)S_IFDIR & (uint64_t)(statbuf.st_mode);
}

/// \brief Remove a file or directory
/// \return Upon successful completion, returns true; otherwise, false is returned.
/// If the file or directory does not exist, true is returned.
inline bool remove_file(const std::string &path) {
#if defined(__cpp_lib_filesystem) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
  std::filesystem::path p(path);
  std::error_code ec;
  [[maybe_unused]] const auto num_removed = std::filesystem::remove_all(p, ec);
  return !ec;
#else
  std::string rm_command("rm -rf " + path);
  const int status = std::system(rm_command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
#endif
}

inline bool free_file_space([[maybe_unused]] const int fd,
                            [[maybe_unused]] const off_t off,
                            [[maybe_unused]] const off_t len) {
#if defined(FALLOC_FL_PUNCH_HOLE) && defined(FALLOC_FL_KEEP_SIZE)
  if (::fallocate(fd, FALLOC_FL_PUNCH_HOLE | FALLOC_FL_KEEP_SIZE, off, len) == -1) {
    logger::perror(logger::level::warning, __FILE__, __LINE__, "fallocate");
    return false;
  }
  return true;

#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "FALLOC_FL_PUNCH_HOLE or FALLOC_FL_KEEP_SIZE is not supported"
#endif
  return false;
#endif
}

#if defined(__cpp_lib_filesystem) && !defined(METALL_NOT_USE_CXX17_FILESYSTEM_LIB)
inline bool copy_file(const std::string &source_path, const std::string &destination_path) {
  bool success = true;
  try {
    if (!fs::copy_file(source_path, destination_path, fs::copy_options::overwrite_existing)) {
      std::stringstream ss;
      ss << "Failed copying file: " << source_path << " -> " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
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
      std::stringstream ss;
      ss << "Cannot open: " << source_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      return false;
    }

    std::ofstream destination(destination_path);
    if (!destination.is_open()) {
      std::stringstream ss;
      ss << "Cannot open: " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      return false;
    }

    destination << source.rdbuf();
    if (!destination) {
      std::stringstream ss;
      ss << "Something happened in the ofstream: " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      return false;
    }

    destination.close();
  }

  {
    // Sanity check
    const ssize_t s1 = get_file_size(source_path);
    const ssize_t s2 = get_file_size(destination_path);
    if (s1 < 0 || s1 != s2) {
      std::stringstream ss;
      ss << "Something wrong in file sizes: " << s1 << " " << s2;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      return false;
    }
  }
  return true;
}

#endif

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY_FILE_HPP
