// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_FILESYSTEM_HPP
#define METALL_UTILITY_FILESYSTEM_HPP

#include <metall/detail/file.hpp>

namespace metall::utility::filesystem {

/// \brief Remove a file or directory
/// \return Upon successful completion, returns true; otherwise, false is
/// returned. If the file or directory does not exist, true is returned.
inline bool remove(std::string_view path) {
  return metall::mtlldetail::remove_file(path.data());
}

}  // namespace metall::utility::filesystem

#endif  // METALL_UTILITY_FILESYSTEM_HPP
