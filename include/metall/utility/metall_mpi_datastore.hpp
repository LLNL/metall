// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_METALL_MPI_DATASTORE_HPP
#define METALL_UTILITY_METALL_MPI_DATASTORE_HPP

#include <string>

/// \namespace metall::utility::mpi_datastore
/// \brief Namespace for MPI datastore
namespace metall::utility::mpi_datastore {

/// \brief Makes a path of a root directory.
/// The MPI ranks that share the same storage space create their data stores
/// under the root directory. \param root_dir_prefix A prefix of a root
/// directory path. \return A path of a root directory.
inline std::string make_root_dir_path(const std::string &root_dir_prefix) {
  return root_dir_prefix + "/";
}

/// \brief Makes the data store path of a MPI rank.
/// \param root_dir_prefix A prefix of a root directory path.
/// \param rank A MPI rank.
/// \return A path to a local data store directory.
/// The path can be passed to, for example, metall::manager to perform
/// operations.
inline std::string make_local_dir_path(const std::string &root_dir_prefix,
                                       const int rank) {
  return make_root_dir_path(root_dir_prefix) + "/subdir-" +
         std::to_string(rank);
}

}  // namespace metall::utility::mpi_datastore

#endif  // METALL_UTILITY_METALL_MPI_DATASTORE_HPP
