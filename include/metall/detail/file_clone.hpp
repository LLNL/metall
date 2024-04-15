// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
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

namespace {
namespace fs = std::filesystem;
}

namespace file_clone_detail {
#ifdef __linux__

/**
 * Clone file using
 * https://man7.org/linux/man-pages/man2/ioctl_ficlone.2.html
 */
inline bool clone_file_linux(const int src, const int dst) {
#ifdef FICLONE
  return ::ioctl(dst, FICLONE, src) != -1;
#else
  errno = ENOTSUP;
  return false;
#endif  // defined(FICLONE)
}

/**
 * Attempts to perform an O(1) clone of source_path to destionation_path
 * if cloning fails, falls back to sparse copying
 * if sparse copying fails, falls back to regular copying
 */
inline bool clone_file_linux(const std::filesystem::path &source_path,
                             const std::filesystem::path &destination_path) {
  int src;
  int dst;
  const off_t src_size = fcpdtl::prepare_file_copy_linux(source_path, destination_path, &src, &dst);
  if (src_size < 0) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Unable to prepare for file copy");
    return false;
  }

  const auto close_fsync_all = [&]() noexcept {
    os_fsync(dst);
    os_close(src);
    os_close(dst);
  };

  if (clone_file_linux(src, dst)) {
    close_fsync_all();
    return true;
  }

  {
    std::stringstream ss;
    ss << "Unable to clone " << source_path << " to " << destination_path << ", falling back to sparse copy";
    logger::out(logger::level::warning, __FILE__, __LINE__, ss.str().c_str());
  }

  if (fcpdtl::copy_file_sparse_linux(src, dst, src_size)) {
    close_fsync_all();
    return true;
  }

  {
    std::stringstream ss;
    ss << "Unable to sparse copy " << source_path << " to " << destination_path << ", falling back to normal copy";
    logger::out(logger::level::warning, __FILE__, __LINE__, ss.str().c_str());
  }

  os_close(src);
  os_close(dst);

  if (fcpdtl::copy_file_dense_linux(source_path, destination_path)) {
    return true;
  }

  std::stringstream ss;
  ss << "Unable to copy " << source_path << " to " << destination_path;
  logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());

  return false;
}

#endif

#ifdef __APPLE__
inline bool clone_file_macos(const fs::path &source_path,
                             const fs::path &destination_path) {
  std::string command("cp -cR " + source_path.string() + " " +
                      destination_path.string());
  const int status = std::system(command.c_str());
  if ((status != -1) && !!(WIFEXITED(status))) {
    return true;
  }

  {
    std::stringstream ss;
    ss << "Unable to sparse copy " << source_path << " to " << destination_path << ", falling back to normal copy";
    logger::out(logger::level::warning, __FILE__, __LINE__, ss.str().c_str());
  }

  return fcpdtl::copy_file_dense(source_path, destination_path);
}
#endif
}  // namespace file_clone_detail

/// \brief Clones a file. If file cloning is not supported, copies the file
/// normally. \param source_path A path to the file to be cloned. \param
/// destination_path A path to copy to. \return On success, returns true. On
/// error, returns false.
inline bool clone_file(const fs::path &source_path,
                       const fs::path &destination_path) {
#if defined(__linux__)
  return file_clone_detail::clone_file_linux(source_path, destination_path);
#elif defined(__APPLE__)
  return file_clone_detail::clone_file_macos(source_path, destination_path);
#else

#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "Copy file normally instead of cloning"
#endif

  logger::out(logger::level::warning, __FILE__, __LINE__,
              "Using normal copy instead of clone");
  return copy_file(source_path, destination_path);

#endif
}

/// \brief Clone files in a directory.
/// This function does not clone files in subdirectories.
/// \param source_dir_path A path to source directory.
/// \param destination_dir_path A path to destination directory.
/// \param max_num_threads The maximum number of threads to use.
/// If <= 0 is given, it is automatically determined.
/// \return  On success, returns true. On error, returns false.
inline bool clone_files_in_directory_in_parallel(
    const fs::path &source_dir_path, const fs::path &destination_dir_path,
    const int max_num_threads) {
  return copy_files_in_directory_in_parallel_helper(
      source_dir_path, destination_dir_path, max_num_threads, clone_file);
}

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_FILE_CLONE_HPP
