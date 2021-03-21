// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_FILE_CLONE_HPP
#define METALL_DETAIL_UTILITY_FILE_CLONE_HPP

#include <sys/ioctl.h>

#ifdef __linux__
#include <linux/fs.h>
#endif

#ifdef __APPLE__
#include <sys/attr.h>
#include <sys/clonefile.h>
#endif

#include <cstdlib>
#include <atomic>
#include <thread>
#include <metall/detail/file.hpp>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

namespace file_clone_detail {
#ifdef __linux__
inline bool clone_file_linux(const std::string& source_path, const std::string& destination_path) {
#if 0
#ifdef FICLONE
  const int source_fd = ::open(source_path.c_str(), O_RDONLY);
  if (source_fd == -1) {
    logger::out(logger::level::error, __FILE__, __LINE__, "open " + source_path);
    return false;
  }

  const int destination_fd = ::open(destination_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (destination_fd == -1) {
    logger::out(logger::level::error, __FILE__, __LINE__, "open " + destination_path);
    return false;
  }

  if (::ioctl(destination_fd, FICLONE, source_fd) == -1) {
    logger::out(logger::level::error, __FILE__, __LINE__, "ioctl + FICLONE");
    return false;
  }

  int ret = true;
  ret &= os_close(source_fd);
  ret &= os_close(destination_fd);

  return ret;
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "ioctl_ficlone is not supported"
  return copy_file(source_path, destination_path); // Copy normally
#endif
#endif
#else
  std::string command("cp --reflink=auto -R " + source_path + " " + destination_path);
  const int status = std::system(command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
#endif
  return true;
}
#endif

#ifdef __APPLE__
inline bool clone_file_macos(const std::string &source_path, const std::string &destination_path) {
#if 0
  if (::clonefile(source_path.c_str(), destination_path.c_str(), 0) == -1) {
    logger::out(logger::level::error, __FILE__, __LINE__, "clonefile");
    return false;
  }
  return true;
#else
  std::string command("cp -cR " + source_path + " " + destination_path);
  const int status = std::system(command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
#endif
}
#endif
}// namespace file_clone_detail

/// \brief Clones a file. If file cloning is not supported, copies the file normally.
/// \param source_path A path to the file to be cloned.
/// \param destination_path A path to copy to.
/// \return On success, returns true. On error, returns false.
inline bool clone_file(const std::string &source_path, const std::string &destination_path) {
  bool ret = false;
#if defined(__linux__)
  ret = file_clone_detail::clone_file_linux(source_path, destination_path);
  if (!ret)
      logger::out(logger::level::error, __FILE__, __LINE__, "On Linux, Failed to clone " + source_path + " to " + destination_path);
#elif defined(__APPLE__)
  ret = file_clone_detail::clone_file_macos(source_path, destination_path);
  if (!ret)
    logger::out(logger::level::error,
                __FILE__,
                __LINE__,
                "On MacOS, Failed to clone " + source_path + " to " + destination_path);
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "Copy file normally instead of cloning"
#endif
  logger::out(logger::level::warning, __FILE__, __LINE__, "Use normal copy instead of clone");
  ret = copy_file(source_path, destination_path); // Copy normally
  if (!ret)
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed to copy " + source_path + " to " + destination_path);
#endif

  if (ret) {
    ret &= metall::mtlldetail::fsync(destination_path);
  }

  return ret;
}

/// \brief Clone files in a directory.
/// This function does not clone files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If 0 is given, it is automatically determined.
/// \return  On success, returns true. On error, returns false.
inline bool clone_files_in_directory_in_parallel(const std::string &source_dir_path,
                                                 const std::string &destination_dir_path,
                                                 const int max_num_threads) {
  return copy_files_in_directory_in_parallel_helper(source_dir_path, destination_dir_path, max_num_threads, clone_file);
}

} // namespace metall::mtlldetail

#endif //METALL_DETAIL_UTILITY_FILE_CLONE_HPP
