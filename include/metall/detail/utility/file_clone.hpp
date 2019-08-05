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

#include <metall/detail/utility/file.hpp>

namespace metall {
namespace detail {
namespace utility {

namespace detail {
#ifdef __linux__
inline bool clone_file_linux(const std::string& source_path, const std::string& destination_path) {
#ifdef FICLONE
  const int source_fd = ::open(source_path.c_str(), O_RDONLY);
  if (source_fd == -1) {
    const std::string err_msg("open " + source_path);
    ::perror(err_msg.c_str());
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  const int destination_fd = ::open(destination_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (destination_fd == -1) {
    const std::string err_msg("open " + destination_path);
    ::perror(err_msg.c_str());
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  if (::ioctl(destination_fd, FICLONE, source_fd) == -1) {
    ::perror("ioctl + FICLONE");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }

  ::close(source_fd);
  ::close(destination_fd);

  return true;
#else
#warning "ioctl_ficlone is not supported"
  return copy_file(source_path, destination_path); // Copy normally
#endif
}
#endif

#ifdef __APPLE__
inline bool clone_file_macos(const std::string& source_path, const std::string& destination_path) {
  if (::clonefile(source_path.c_str(), destination_path.c_str(), 0) == -1) {
    ::perror("clonefile");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}
#endif
}// namespace detail

/// \brief Clones a file. If file cloning is not supported, copies the file normally
/// \param source_path A path to the file to be cloned
/// \param destination_path A path to copy to
/// \return On success, returns true. On error, returns false.
inline bool clone_file(const std::string& source_path, const std::string& destination_path, const bool sync) {
  bool ret = false;
#if defined(__linux__)
  ret = detail::clone_file_linux(source_path, destination_path);
#elif defined(__APPLE__)
  ret = detail::clone_file_macos(source_path, destination_path);
#else
  #warning "Copy file normally instead of cloning"
  ret = copy_file(source_path, destination_path); // Copy normally
#endif

  if(ret && sync) {
    ret &= fsync(destination_path);
  }

  return ret;
}

} // namespace metall
} // namespace detail
} // namespace utility

#endif //METALL_DETAIL_UTILITY_FILE_CLONE_HPP
