// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_VERSION_HPP
#define METALL_VERSION_HPP

/// \brief Metall version.
/// Metall follows Semantic Versioning 2.0.0
/// https://semver.org/#semantic-versioning-200.
/// \details
/// \code
///  METALL_VERSION / 100000 // the major version.
///  METALL_VERSION / 100 % 1000 // the minor version.
///  METALL_VERSION % 100 // the patch level.
/// \endcode
#define METALL_VERSION 600

namespace metall {
/// \brief Variable type to handle a version data.
using version_type = int32_t;
static_assert(std::numeric_limits<version_type>::max() >= METALL_VERSION,
              "version_type cannot handle the current version");

/// \brief Convert a raw version number to a std::string with the following format: 'MAJOR.MINOR.PATCH'.
/// \param version A version number.
/// \return A version string.
inline std::string to_version_string(const version_type version) {
  return std::to_string(version / 100000)
      + "." + std::to_string(version / 100 % 1000)
      + "." + std::to_string(version % 100);
}
}

#endif //METALL_VERSION_HPP
