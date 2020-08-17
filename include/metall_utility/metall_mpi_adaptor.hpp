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
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::open_read_only_t, const std::string &data_store_dir, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_read_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::create_only_t, const std::string &data_store_dir,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    setup_inter_level_dir(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::create_only, m_local_dir_path.c_str());
  }

  metall_mpi_adaptor(metall::create_only_t, const std::string &data_store_dir, const std::size_t capacity,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_local_dir_path(make_local_dir_path(data_store_dir, comm)),
        m_local_metall_manager(nullptr) {
    setup_inter_level_dir(data_store_dir, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::create_only, m_local_dir_path.c_str(), capacity);
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

  static bool copy(const char *source_dir_path,
                   const char *destination_dir_path,
                   const MPI_Comm &comm = MPI_COMM_WORLD) {
    setup_inter_level_dir(destination_dir_path, comm);
    return global_or(manager_type::copy(make_local_dir_path(source_dir_path, comm).c_str(),
                                        make_local_dir_path(destination_dir_path, comm).c_str()), comm);
  }

  bool snapshot(const char *destination_dir_path) {
    setup_inter_level_dir(destination_dir_path, m_mpi_comm);
    return global_or(m_local_metall_manager->snapshot(make_local_dir_path(destination_dir_path, m_mpi_comm).c_str()),
                     m_mpi_comm);
  }

  static bool remove(const char *dir_path, const MPI_Comm &comm = MPI_COMM_WORLD) {
    return manager_type::remove(make_local_dir_path(dir_path, comm).c_str());
  }

 private:
  static constexpr const char *k_datastore_inter_level_dir_name = "metall_mpi_datastore";

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  static void setup_inter_level_dir(const std::string &base_dir_path, const MPI_Comm &comm) {
    const int rank = mpi_comm_rank(comm);

    for (int i = 0; i < mpi_comm_size(comm); ++i) {
      if (i == rank) {
        const std::string path = make_inter_level_dir_path(base_dir_path);
        if (!metall::detail::utility::file_exist(path) && !metall::detail::utility::create_directory(path)) {
          std::cerr << "Failed to create directory: " << path << std::endl;
          ::MPI_Abort(comm, -1);
        }
      }
      mpi_barrier(comm);
    }
  }

  static std::string make_inter_level_dir_path(const std::string &base_dir_path) {
    return base_dir_path + "/" + k_datastore_inter_level_dir_name;
  }

  static std::string make_local_dir_path(const std::string &base_dir_path, const MPI_Comm &comm) {
    const int rank = mpi_comm_rank(comm);
    return make_inter_level_dir_path(base_dir_path) + "/subdir-" + std::to_string(rank);
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

  static bool global_or(const bool local_result, const MPI_Comm &comm) {
    char global_result = 0;
    if (::MPI_Allreduce(&local_result, &global_result, 1, MPI_CHAR, MPI_LAND, comm) != MPI_SUCCESS) {
      return false;
    }
    return global_result;
  }

  MPI_Comm m_mpi_comm;
  std::string m_local_dir_path;
  std::unique_ptr<manager_type> m_local_metall_manager;
};

} // namespace metall_utility

#endif //METALL_METALL_MPI_ADAPTOR_HPP
