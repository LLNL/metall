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
#include <metall/detail/file.hpp>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

namespace file_clone_detail {
#ifdef __linux__
inline bool clone_file_linux(const std::string& source_path, const std::string& destination_path) {
  std::string command("cp --reflink=auto -R " + source_path + " " + destination_path);
  const int status = std::system(command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
}
#endif

#ifdef __APPLE__
inline bool clone_file_macos(const std::string &source_path, const std::string &destination_path) {
  std::string command("cp -cR " + source_path + " " + destination_path);
  const int status = std::system(command.c_str());
  return (status != -1) && !!(WIFEXITED(status));
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
  if (!ret) {
    std::string s("On Linux, Failed to clone " + source_path + " to " + destination_path);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
  }
#elif defined(__APPLE__)
  ret = file_clone_detail::clone_file_macos(source_path, destination_path);
  if (!ret) {
    std::string s("On MacOS, Failed to clone " + source_path + " to " + destination_path);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
  }
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "Copy file normally instead of cloning"
#endif
  logger::out(logger::level::warning, __FILE__, __LINE__, "Use normal copy instead of clone");
  ret = copy_file(source_path, destination_path); // Copy normally
  if (!ret) {
    std::string s("Failed to copy " + source_path + " to " + destination_path);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
  }
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
/// If <= 0 is given, it is automatically determined.
/// \return  On success, returns true. On error, returns false.
inline bool clone_files_in_directory_in_parallel(const std::string &source_dir_path,
                                                 const std::string &destination_dir_path,
                                                 const int max_num_threads) {
  return copy_files_in_directory_in_parallel_helper(source_dir_path, destination_dir_path, max_num_threads, clone_file);
}

} // namespace metall::mtlldetail

#endif //METALL_DETAIL_UTILITY_FILE_CLONE_HPP
