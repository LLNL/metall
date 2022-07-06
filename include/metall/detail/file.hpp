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
#include <dirent.h>
#include <libgen.h>

#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <cstdlib>
#include <ctime>
#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <functional>

#ifdef __has_include

// Check if the Filesystem library is available or disabled by the user
#if __has_include(<filesystem>) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
#include <filesystem>
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "The Filesystem library is not available or disabled by the user."
#endif
#endif

#else // __has_include is not defined

#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "__has_include is not defined, consequently disable the Filesystem library."
#endif // METALL_VERBOSE_SYSTEM_SUPPORT_WARNING

#endif // #ifdef __has_include

#include <metall/logger.hpp>

namespace metall::mtlldetail {

#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
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
#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
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
  char *abs = ::realpath(path.c_str(), NULL);
  if (!abs) return false;
  char *ref = abs;
  while (true) {
    if (!fsync(std::string(abs))) {
      ::free(ref);
      return false;
    }
    if (::strcmp(abs, "/") == 0) {
      break;
    }
    abs = ::dirname(abs);
  }
  ::free(ref);
  return true;
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
    struct stat stat_buf;
    if (::fstat(fd, &stat_buf) == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "fstat");
      return false;
    }
    if (::llabs(stat_buf.st_size) < static_cast<ssize_t>(file_size)) {
      if (::ftruncate(fd, file_size) == -1) {
        logger::perror(logger::level::error, __FILE__, __LINE__, "ftruncate");
        return false;
      }
    }
  }

  const bool ret = os_fsync(fd);
  return ret;
}

inline bool extend_file_size(const std::string &file_path, const size_t file_size, const bool fill_with_zero = false) {
  const int fd = ::open(file_path.c_str(), O_RDWR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return false;
  }

  bool ret = extend_file_size(fd, file_size, fill_with_zero);
  ret &= os_close(fd);

  return ret;
}

/// \brief Check if a file, any kinds of file including directory, exists
/// \warning This implementation could return a wrong result due to metadata cache on NFS.
/// The following code could fail:
/// if (mpi_rank == 1) file_exist(path); // NFS creates metadata cache
/// mpi_barrier();
/// if (mpi_rank == 0) create_directory(path);
/// mpi_barrier();
/// if (mpi_rank == 1) assert(file_exist(path)); // Could fail due to the cached metadata.
inline bool file_exist(const std::string &file_name) {
  std::string fixed_string(file_name);
  while (fixed_string.back() == '/') {
    fixed_string.pop_back();
  }
  return (::access(fixed_string.c_str(), F_OK) == 0);
}

/// \brief Check if a directory exists
/// \warning This implementation could return a wrong result due to metadata cache on NFS.
inline bool directory_exist(const std::string &dir_path) {
  struct stat stat_buf;
  if (::stat(dir_path.c_str(), &stat_buf) == -1) {
    return false;
  }
  return S_ISDIR(stat_buf.st_mode);
}

inline bool create_file(const std::string &file_path) {

  const int fd = ::open(file_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return false;
  }

  if (!os_close(fd))
    return false;

  return fsync_recursive(file_path);
}

#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
/// \brief Creates directories recursively.
/// \return Returns true if the directory was created or already exists.
/// Otherwise, returns false.
inline bool create_directory(const std::string &dir_path) {

  std::string fixed_string = dir_path;
  // MEMO: GCC bug 87846 (fixed in v8.3)
  // "Calling std::filesystem::create_directories with a path with a trailing separator (e.g. "./a/b/")
  // does not create any directory."
#if (defined(__GNUG__) && !defined(__clang__)) && (__GNUC__ < 8 || (__GNUC__ == 8 && __GNUC_MINOR__ < 3)) // Check if < GCC 8.3
  // Remove trailing separator(s) if they exist:
    while (fixed_string.back() == '/') {
      fixed_string.pop_back();
    }
#endif

  bool success = true;
  try {
    std::error_code ec;
    if (!fs::create_directories(fixed_string, ec)) {
      if (!ec) {
        // if the directory exist, create_directories returns false.
        // However, std::error_code is cleared and !ec returns true.
        return true;
      }

      logger::out(logger::level::error, __FILE__, __LINE__, ec.message().c_str());
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    success = false;
  }

  return success;
}
#else
/// \brief Creates directories recursively.
/// \return Returns true if the directory was created or already exists, returns true.
/// Otherwise, returns false.
inline bool create_directory(const std::string &dir_path) {

  std::string mkdir_command("mkdir -p " + dir_path);
  const int status = std::system(mkdir_command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
}
#endif

inline ssize_t get_file_size(const std::string &file_path) {
  std::ifstream ifs(file_path, std::ifstream::binary | std::ifstream::ate);
  ssize_t size = ifs.tellg();
  if (size == -1) {
    std::stringstream ss;
    ss << "Failed to get file size: " << file_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
  }

  return size;
}

/// \brief
/// Note that, according to GCC,
/// the file system may use some blocks for internal record keeping
inline ssize_t get_actual_file_size(const std::string &file_path) {
  struct stat stat_buf;
  if (::stat(file_path.c_str(), &stat_buf) != 0) {
    std::string s("stat (" + file_path + ")");
    logger::perror(logger::level::error, __FILE__, __LINE__, s.c_str());
    return -1;
  }
  return stat_buf.st_blocks * 512LL;
}

/// \brief Remove a file or directory
/// \return Upon successful completion, returns true; otherwise, false is returned.
/// If the file or directory does not exist, true is returned.
inline bool remove_file(const std::string &path) {
#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
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

namespace file_copy_detail {

#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)
inline bool copy_file_dense(const std::string &source_path, const std::string &destination_path) {
  bool success = true;
  try {
    if (!fs::copy_file(source_path, destination_path, fs::copy_options::overwrite_existing)) {
      std::stringstream ss;
      ss << "Failed copying file: " << source_path << " -> " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      success = false;
    }
  } catch (fs::filesystem_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    success = false;
  }

  if (success) {
    success &= metall::mtlldetail::fsync(destination_path);
  }

  return success;
}

#else

inline bool copy_file_dense(const std::string &source_path, const std::string &destination_path) {
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
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    std::ofstream destination(destination_path);
    if (!destination.is_open()) {
      std::stringstream ss;
      ss << "Cannot open: " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    destination << source.rdbuf();
    if (!destination) {
      std::stringstream ss;
      ss << "Something happened in the ofstream: " << destination_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    destination.close();

    if (!metall::mtlldetail::fsync(destination_path)) {
      return false;
    }
  }

  {
    // Sanity check
    const ssize_t s1 = get_file_size(source_path);
    const ssize_t s2 = get_file_size(destination_path);
    if (s1 < 0 || s1 != s2) {
      std::stringstream ss;
      ss << "Something wrong in file sizes: " << s1 << " " << s2;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }
  }
  return true;
}

#endif

#ifdef __linux__
inline bool copy_file_sparse_linux(const std::string &source_path, const std::string &destination_path) {
  std::string command("cp --sparse=auto " + source_path + " " + destination_path);
  const int status = std::system(command.c_str());
  const bool success = (status != -1) && !!(WIFEXITED(status));
  if (!success) {
    std::stringstream ss;
    ss << "Failed copying file: " << source_path << " -> " << destination_path;
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return false;
  }
  return success;
}
#endif

}// namespace file_copy_detail

/// \brief Copy a file.
/// \param source_path A source file path.
/// \param destination_path A destination path.
/// \param sparse_copy If true is specified, tries to perform sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline bool copy_file(const std::string &source_path,
                      const std::string &destination_path,
                      const bool sparse_copy = true) {
  if (sparse_copy) {
#ifdef __linux__
    return file_copy_detail::copy_file_sparse_linux(source_path, destination_path);
#else
    logger::out(logger::level::info, __FILE__, __LINE__, "Sparse file copy is not available");
#endif
  }
  return file_copy_detail::copy_file_dense(source_path, destination_path);
}

/// \brief Get the file names in a directory.
/// This function does not list files recursively.
/// Only regular files are returned.
/// \param dir_path A directory path.
/// \param file_list A buffer to put results.
/// \return Returns true if there is no error (empty directory returns true as long as the operation does not fail).
/// Returns false on error.
inline bool get_regular_file_names(const std::string &dir_path, std::vector<std::string> *file_list) {
#if defined(__cpp_lib_filesystem) && !defined(METALL_DISABLE_CXX17_FILESYSTEM_LIB)

  if (!directory_exist(dir_path)) {
    return false;
  }

  try {
    file_list->clear();
    for (auto &p: fs::directory_iterator(dir_path)) {
      if (p.is_regular_file()) {
        file_list->push_back(p.path().filename().string());
      }
    }
  } catch (...) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Exception was thrown");
    return false;
  }

  return true;

#else
  DIR *d = ::opendir(dir_path.c_str());
  if (!d) {
    return false;
  }

  for (dirent *dir; (dir = ::readdir(d)) != nullptr;) {
    if (dir->d_type == DT_REG) {
      file_list->push_back(dir->d_name);
    }
  }
  ::closedir(d);

  return true;
#endif
}

/// \brief Copy files in a directory.
/// This function does not copy files in subdirectories.
/// This function does not also copy directories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, the value is automatically determined.
/// \param copy_func The actual copy function.
/// \return  On success, returns true. On error, returns false.
inline bool
copy_files_in_directory_in_parallel_helper(const std::string &source_dir_path,
                                           const std::string &destination_dir_path,
                                           const int max_num_threads,
                                           const std::function<bool(const std::string &,
                                                                    const std::string &)> &copy_func) {

  std::vector<std::string> src_file_names;
  if (!get_regular_file_names(source_dir_path, &src_file_names)) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed to get file list");
    return false;
  }

  std::atomic_uint_fast64_t num_successes = 0;
  std::atomic_uint_fast64_t file_no_cnt = 0;
  auto copy_lambda
      = [&file_no_cnt, &num_successes, &source_dir_path, &src_file_names, &destination_dir_path, &copy_func]() {
        while (true) {
          const auto file_no = file_no_cnt.fetch_add(1);
          if (file_no >= src_file_names.size()) break;
          const std::string &src_file_path = source_dir_path + "/" + src_file_names[file_no];
          const std::string &dst_file_path = destination_dir_path + "/" + src_file_names[file_no];
          num_successes.fetch_add(copy_func(src_file_path, dst_file_path) ? 1 : 0);
        }
      };

  const auto num_threads = (int)std::min(src_file_names.size(),
                                         (std::size_t)(max_num_threads > 0 ? max_num_threads
                                                                           : std::thread::hardware_concurrency()));
  std::vector<std::thread *> threads(num_threads, nullptr);
  for (auto &th: threads) {
    th = new std::thread(copy_lambda);
  }

  for (auto &th: threads) {
    th->join();
  }

  return num_successes == src_file_names.size();
}

/// \brief Copy files in a directory.
/// This function does not copy files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \param sparse_copy Performs sparse file copy.
/// \return  On success, returns true. On error, returns false.
inline bool copy_files_in_directory_in_parallel(const std::string &source_dir_path,
                                                const std::string &destination_dir_path,
                                                const int max_num_threads,
                                                const bool sparse_copy = true) {
  return copy_files_in_directory_in_parallel_helper(source_dir_path,
                                                    destination_dir_path,
                                                    max_num_threads,
                                                    [&sparse_copy](const std::string &src,
                                                                   const std::string &dst) -> bool {
                                                      return copy_file(src, dst, sparse_copy);
                                                    });
}
} // namespace metall::mtlldetail

#endif //METALL_DETAIL_UTILITY_FILE_HPP
