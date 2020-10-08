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
  /// \brief Opens an existing Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::open_only_t, const std::string &root_dir_prefix, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_verify_num_partitions(root_dir_prefix, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_only,
                                                            priv_make_local_dir_path(m_root_dir_prefix,
                                                                                     priv_mpi_comm_rank(comm)).c_str());
  }

  /// \brief Opens an existing Metall datastore with the read-only mode.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::open_read_only_t, const std::string &root_dir_prefix, const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_verify_num_partitions(root_dir_prefix, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_read_only,
                                                            priv_make_local_dir_path(m_root_dir_prefix,
                                                                                     priv_mpi_comm_rank(comm)).c_str());
  }

  /// \brief Creates a new Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// The same name of file or directory must not exist.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::create_only_t, const std::string &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_setup_root_dir(root_dir_prefix, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::create_only,
                                                            priv_make_local_dir_path(m_root_dir_prefix,
                                                                                     priv_mpi_comm_rank(comm)).c_str());
  }

  /// \brief Creates a new Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// The same name of file or directory must not exist.
  /// \param capacity The max capacity of the datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::create_only_t, const std::string &root_dir_prefix, const std::size_t capacity,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_setup_root_dir(root_dir_prefix, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::create_only,
                                                            priv_make_local_dir_path(m_root_dir_prefix,
                                                                                     priv_mpi_comm_rank(comm)).c_str(),
                                                            capacity);
  }

  /// \brief Destructor that globally synchronizes the close operations of all sub-Metall datastores.
  ~metall_mpi_adaptor() {
    m_local_metall_manager.reset(nullptr);
    priv_mpi_barrier(m_mpi_comm);
  }

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //

  /// \brief Returns the Metall manager object of the process.
  /// \return A reference to a Metall manager object.
  manager_type &get_local_manager() {
    return *m_local_metall_manager;
  }

  /// \brief Returns the Metall manager object of the process.
  /// \return A reference to a Metall manager object.
  const manager_type &get_local_manager() const {
    return *m_local_metall_manager;
  }

  /// \brief Returns the root path of a Metall datastore
  /// \return A root path of a Metall datastore.
  std::string root_dir_path() const {
    return priv_make_root_dir_path(m_root_dir_prefix);
  }

  /// \brief Returns the path of the sub-Metall datastore of the process.
  /// \return A path of a sub-Metall datastore.
  std::string local_dir_path() const {
    return priv_make_local_dir_path(priv_make_root_dir_path(m_root_dir_prefix), priv_mpi_comm_rank(m_mpi_comm));
  }

  /// \brief Returns the path of a Metall datastore of a MPI rank.
  /// \param root_dir_prefix A root directory path.
  /// \param mpi_rank A MPI rank.
  /// \return A path of a Metall datastore.
  static std::string local_dir_path(const std::string &root_dir_prefix, const int mpi_rank) {
    return priv_make_local_dir_path(root_dir_prefix, mpi_rank);
  }

  /// \brief Copies a Metall datastore to another location.
  /// \param source_dir_path A path to a source datastore.
  /// \param destination_dir_path A path to a destination datastore.
  /// \param comm A MPI communicator.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  static bool copy(const char *source_dir_path,
                   const char *destination_dir_path,
                   const MPI_Comm &comm = MPI_COMM_WORLD) {
    priv_setup_root_dir(destination_dir_path, comm);
    const int rank = priv_mpi_comm_rank(comm);
    return priv_global_and(manager_type::copy(priv_make_local_dir_path(source_dir_path, rank).c_str(),
                                              priv_make_local_dir_path(destination_dir_path, rank).c_str()), comm);
  }

  /// \brief Take a snapshot of the current Metall datastore to another location.
  /// \param destination_dir_path A path to a destination datastore.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  bool snapshot(const char *destination_dir_path) {
    priv_setup_root_dir(destination_dir_path, m_mpi_comm);
    const int rank = priv_mpi_comm_rank(m_mpi_comm);
    return priv_global_and(m_local_metall_manager->snapshot(priv_make_local_dir_path(destination_dir_path,
                                                                                     rank).c_str()),
                           m_mpi_comm);
  }

  /// \brief Removes Metall datastore.
  /// \param root_dir_prefix A root directory path of datastore.
  /// \param comm A MPI communicator.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  static bool remove(const char *root_dir_prefix, const MPI_Comm &comm = MPI_COMM_WORLD) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    // ----- Check if this is a Metall datastore ----- //
    bool corrent_dir = true;
    if (!metall::detail::utility::file_exist(
        priv_make_root_dir_path(root_dir_prefix) + "/" + k_datastore_mark_file_name)) {
      corrent_dir = false;
    }
    if (!priv_global_and(corrent_dir, comm)) {
      return false;
    }

    // ----- Check if #of MPI processes matches ----- //
    bool correct_mpi_size = true;
    if (rank == 0) {
      const int read_size = priv_read_num_partitions(root_dir_prefix, comm);
      if (read_size != size) {
        correct_mpi_size = false;
        std::cerr << __FILE__ << " : " << __func__ << " Invalid number of MPI processes "
                  << "(provided " << size << ", " << "expected " << correct_mpi_size << ")" << std::endl;
      }
    }
    if (!priv_global_and(correct_mpi_size, comm)) {
      return false;
    }

    // ----- Remove directories ----- //
    bool ret = true;
    for (int i = 0; i < size; ++i) {
      if (i == rank) {
        if (metall::detail::utility::file_exist(priv_make_root_dir_path(root_dir_prefix))
            && !metall::detail::utility::remove_file(priv_make_root_dir_path(root_dir_prefix))) {
          std::cerr << __FILE__ << " : " << __func__ << " Failed to remove directory: "
                    << priv_make_root_dir_path(root_dir_prefix) << std::endl;
          ret = false;
        }
      }
      priv_mpi_barrier(comm);
    }

    return priv_global_and(ret, comm);
  }

  /// \brief Returns the number of partition of a Metall datastore.
  /// \param root_dir_path A root directory path of datastore.
  /// \param comm A MPI communicator.
  /// \return The number of partitions of a Metall datastore.
  static int partitions(const char *root_dir_path, const MPI_Comm &comm = MPI_COMM_WORLD) {
    return priv_read_num_partitions(root_dir_path, comm);
  }

 private:
  static constexpr const char *k_datastore_mark_file_name = "metall_mpi_datastore";
  static constexpr const char *k_partition_size_file_name = "metall_mpi_adaptor_partition_size";

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  static void priv_setup_root_dir(const std::string &root_dir_path, const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    if (metall::detail::utility::file_exist(priv_make_root_dir_path(root_dir_path))) {
      std::cerr << __FILE__ << " : " << __func__ << " Root directory already exists "
                << priv_make_root_dir_path(root_dir_path) << std::endl;
      ::MPI_Abort(comm, -1);
    }
    priv_mpi_barrier(comm);

    // To simplify the code, all processes do the same thing.
    for (int i = 0; i < size; ++i) {
      if (i == rank) {
        const std::string path = priv_make_root_dir_path(root_dir_path);
        if (!metall::detail::utility::file_exist(path) && !metall::detail::utility::create_directory(path)) {
          std::cerr << __FILE__ << " : " << __func__ << " Failed to create directory: " << path << std::endl;
          ::MPI_Abort(comm, -1);
        }

        const std::string mark_file = path + "/" + k_datastore_mark_file_name;
        if (!metall::detail::utility::file_exist(mark_file) && !metall::detail::utility::create_file(mark_file)) {
          std::cerr << __FILE__ << " : " << __func__ << " Failed to create file: " << mark_file << std::endl;
          ::MPI_Abort(comm, -1);
        }

        priv_store_partition_size(root_dir_path, comm);
      }
      priv_mpi_barrier(comm);
    }
  }

  static void priv_store_partition_size(const std::string &root_dir_path, const MPI_Comm &comm) {
    const int size = priv_mpi_comm_size(comm);
    const std::string path = priv_make_root_dir_path(root_dir_path) + "/" + k_partition_size_file_name;

    std::ofstream ofs(path);
    if (!ofs) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed to create a file: " << path << std::endl;
      ::MPI_Abort(comm, -1);
    }
    ofs << size;
    if (!ofs) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed to write data to a file: " << path << std::endl;
      ::MPI_Abort(comm, -1);
    }
    ofs.close();
  }

  static int priv_read_num_partitions(const std::string &root_dir_prefix, const MPI_Comm &comm) {
    const std::string path = priv_make_root_dir_path(root_dir_prefix) + "/" + k_partition_size_file_name;
    std::ifstream ifs(path);
    if (!ifs) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed to open a file: " << path << std::endl;
      ::MPI_Abort(comm, -1);
    }
    int read_size = -1;
    if (!(ifs >> read_size)) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed to read data from: " << path << std::endl;
      ::MPI_Abort(comm, -1);
    }

    return read_size;
  }

  static void priv_verify_num_partitions(const std::string &root_dir_path, const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    if (rank == 0) {
      if (priv_read_num_partitions(root_dir_path, comm) != size) {
        std::cerr << __FILE__ << " : " << __func__ << " Invalid number of MPI processes" << std::endl;
        ::MPI_Abort(comm, -1);
      }
    }
    priv_mpi_barrier(comm);
  }

  static std::string priv_make_root_dir_path(const std::string &root_dir_path) {
    return root_dir_path + "/";
  }

  static std::string priv_make_local_dir_path(const std::string &root_dir_prefix, const int rank) {
    return priv_make_root_dir_path(root_dir_prefix) + "/subdir-" + std::to_string(rank);
  }

  static int priv_mpi_comm_rank(const MPI_Comm &comm) {
    int rank;
    if (::MPI_Comm_rank(comm, &rank) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Comm_rank" << std::endl;
      ::MPI_Abort(comm, -1);
    }
    return rank;
  }

  static int priv_mpi_comm_size(const MPI_Comm &comm) {
    int num_ranks;
    if (::MPI_Comm_size(comm, &num_ranks) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Comm_size" << std::endl;
      ::MPI_Abort(comm, -1);
    }
    return num_ranks;
  }

  static void priv_mpi_barrier(const MPI_Comm &comm) {
    if (::MPI_Barrier(comm) != MPI_SUCCESS) {
      std::cerr << __FILE__ << " : " << __func__ << " Failed MPI_Barrier" << std::endl;
      ::MPI_Abort(comm, -1);
    }
  }

  static bool priv_global_and(const bool local_result, const MPI_Comm &comm) {
    char global_result = 0;
    if (::MPI_Allreduce(&local_result, &global_result, 1, MPI_CHAR, MPI_LAND, comm) != MPI_SUCCESS) {
      return false;
    }
    return global_result;
  }

  MPI_Comm m_mpi_comm;
  std::string m_root_dir_prefix;
  std::unique_ptr<manager_type> m_local_metall_manager;
};

} // namespace metall_utility

#endif //METALL_METALL_MPI_ADAPTOR_HPP
