// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_VERSION_HPP
#define METALL_VERSION_HPP

/// \brief Metall version.
/// \details
/// \code
///  METALL_VERSION / 100000 // the major version.
///  METALL_VERSION / 100 % 1000 // the minor version.
///  METALL_VERSION % 100 // the patch level.
/// \endcode
#define METALL_VERSION 000600

namespace metall {
/// \brief Variable type to handle a version data.
using version_type = int32_t;
static_assert(std::numeric_limits<version_type>::max() >= METALL_VERSION,
              "version_type cannot handle the current version");
}

#endif //METALL_VERSION_HPP
