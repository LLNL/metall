// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_METALL_MPI_ADAPTOR_HPP
#define METALL_METALL_MPI_ADAPTOR_HPP

#include <mpi.h>

#include <metall/metall.hpp>
#include <metall/detail/utility/file.hpp>

namespace metall::utility {

/// \brief A utility class for using Metall with MPI
/// This is an experimental implementation
class metall_mpi_adaptor {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  /// \brief Metall manager type
  using manager_type = metall::manager;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  metall_mpi_adaptor(metall::open_only_t, const std::string &data_store_dir, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    const auto ret = make_local_dir_path(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::open_read_only_t, const std::string &data_store_dir, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    const auto local_dir_path = make_local_dir_path(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_read_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::create_only_t, const std::string &data_store_dir, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    find_or_create_top_level_dir(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::create_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::open_or_create_t,
                     const std::string &data_store_dir,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    find_or_create_top_level_dir(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_or_create, m_local_dir_path.c_str());
  }

  ~metall_mpi_adaptor() {
    m_local_metall_manager.reset(nullptr); // Destruct the local graph
    mpi_barrier(m_mpi_comm);
  }

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  manager_type &get_local_manager() {
    return *m_local_metall_manager;
  }

  const manager_type &get_local_manager() const {
    return *m_local_metall_manager;
  }

  std::string local_dir_path() const {
    return m_local_dir_path;
  }

  static bool copy(const char *source_dir_path, const char *destination_dir_path, const MPI_Comm &comm = MPI_COMM_WORLD) {
    const auto success = manager_type::copy(make_local_dir_path(source_dir_path, comm).c_str(),
                                            make_local_dir_path(destination_dir_path, comm).c_str());
    char local = success ? 1 : 0;
    char global = 0;
    if (::MPI_Allreduce(&local, &global, 1, MPI_CHAR, MPI_LAND, comm) != MPI_SUCCESS) {
      return false;
    }
    return global;
  }

  bool snapshot(const char *destination_dir_path) {
    const auto success =
        m_local_metall_manager->snapshot(make_local_dir_path(destination_dir_path, m_mpi_comm).c_str());

    char local = success ? 1 : 0;
    char global = 0;
    if (::MPI_Allreduce(&local, &global, 1, MPI_CHAR, MPI_LAND, m_mpi_comm) != MPI_SUCCESS) {
      return false;
    }
    return global;
  }

 private:
  static constexpr const char *k_datastore_top_level_dir_name = "metall_mpi_datastore";

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  static void find_or_create_top_level_dir(const std::string &base_dir_path, const MPI_Comm &comm) {
    const int rank = mpi_comm_rank(comm);

    for (int i = 0; i < mpi_comm_size(comm); ++i) {
      if (i == rank) {
        const std::string full_path = base_dir_path + "/" + k_datastore_top_level_dir_name;
        if (!metall::detail::utility::file_exist(full_path) && !metall::detail::utility::create_directory(full_path)) {
          std::cerr << "Failed to create directory: " << full_path << std::endl;
          ::MPI_Abort(comm, -1);
        }
      }
      mpi_barrier(comm);
    }
  }

  static std::string make_local_dir_path(const std::string &base_dir_path, const MPI_Comm &comm) {
    const int rank = mpi_comm_rank(comm);
    return base_dir_path + "/" + k_datastore_top_level_dir_name + "/subdir-" + std::to_string(rank);
  }

  static int mpi_comm_rank(const MPI_Comm &comm) {
    int rank;
    if (::MPI_Comm_rank(comm, &rank) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Comm_rank" << std::endl;
      ::MPI_Abort(comm, -1);
    }
    return rank;
  }

  static int mpi_comm_size(const MPI_Comm &comm) {
    int num_ranks;
    if (::MPI_Comm_size(comm, &num_ranks) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Comm_size" << std::endl;
      ::MPI_Abort(comm, -1);
    }
    return num_ranks;
  }

  static void mpi_barrier(const MPI_Comm &comm) {
    if (::MPI_Barrier(comm) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Barrier" << std::endl;
      ::MPI_Abort(comm, -1);
    }
  }

  MPI_Comm m_mpi_comm;
  std::string m_local_dir_path;
  std::unique_ptr<manager_type> m_local_metall_manager;
};

} // namespace metall_utility

#endif //METALL_METALL_MPI_ADAPTOR_HPP
