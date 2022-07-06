// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_VERSION_HPP
#define METALL_VERSION_HPP

/// \brief Metall version.
/// Metall follows the Semantic Versioning 2.0.0.
/// \details
/// \code
///  METALL_VERSION / 100000 // the major version.
///  METALL_VERSION / 100 % 1000 // the minor version.
///  METALL_VERSION % 100 // the patch level.
/// \endcode
#define METALL_VERSION 2100

namespace metall {
/// \brief Variable type to handle a version data.
using version_type = uint32_t;
static_assert(std::numeric_limits<version_type>::max() >= METALL_VERSION,
              "version_type cannot handle the current version");

#ifndef DOXYGEN_SKIP
/// \namespace metall::ver_detail
/// \brief Namespace for the details of Metall version
namespace ver_detail {
static constexpr version_type k_error_version = 0;
static_assert(k_error_version != METALL_VERSION, "The current version is equal to a error number");
}
#endif // DOXYGEN_SKIP

/// \brief Converts a raw version number to a std::string with format 'MAJOR.MINOR.PATCH'.
/// \details
/// \code
/// // Example usage
/// std::string ver_string = to_version_string(METALL_VERSION);
/// \endcode
/// \param version A version number.
/// \return A version string.
inline std::string to_version_string(const version_type version) {
  return std::to_string(version / 100000)
      + "." + std::to_string(version / 100 % 1000)
      + "." + std::to_string(version % 100);
}
} // namespace metall

#endif //METALL_VERSION_HPP
