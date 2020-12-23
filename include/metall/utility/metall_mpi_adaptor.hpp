// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_METALL_MPI_ADAPTOR_HPP
#define METALL_UTILITY_METALL_MPI_ADAPTOR_HPP

#include <sstream>

#include <metall/metall.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/utility/mpi.hpp>
#include <metall/utility/metall_mpi_datastore.hpp>

namespace metall::utility {

namespace {
namespace ds = metall::utility::mpi_datastore;
}

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
                                                            ds::make_local_dir_path(m_root_dir_prefix,
                                                                                     priv_mpi_comm_rank(comm)).c_str());
  }

  /// \brief Opens an existing Metall datastore with the read-only mode.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::open_read_only_t,
                     const std::string &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_verify_num_partitions(root_dir_prefix, comm);
    m_local_metall_manager = std::make_unique<manager_type>(metall::open_read_only,
                                                            ds::make_local_dir_path(m_root_dir_prefix,
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
                                                            ds::make_local_dir_path(m_root_dir_prefix,
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
                                                            ds::make_local_dir_path(m_root_dir_prefix,
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
    return ds::make_root_dir_path(m_root_dir_prefix);
  }

  /// \brief Returns the path of the sub-Metall datastore of the process.
  /// \return A path of a sub-Metall datastore.
  std::string local_dir_path() const {
    return ds::make_local_dir_path(ds::make_root_dir_path(m_root_dir_prefix), priv_mpi_comm_rank(m_mpi_comm));
  }

  /// \brief Returns the path of a Metall datastore of a MPI rank.
  /// \param root_dir_prefix A root directory path.
  /// \param mpi_rank A MPI rank.
  /// \return A path of a Metall datastore.
  static std::string local_dir_path(const std::string &root_dir_prefix, const int mpi_rank) {
    return ds::make_local_dir_path(root_dir_prefix, mpi_rank);
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
    return priv_global_and(manager_type::copy(ds::make_local_dir_path(source_dir_path, rank).c_str(),
                                              ds::make_local_dir_path(destination_dir_path, rank).c_str()), comm);
  }

  /// \brief Take a snapshot of the current Metall datastore to another location.
  /// \param destination_dir_path A path to a destination datastore.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  bool snapshot(const char *destination_dir_path) {
    priv_setup_root_dir(destination_dir_path, m_mpi_comm);
    const int rank = priv_mpi_comm_rank(m_mpi_comm);
    return priv_global_and(m_local_metall_manager->snapshot(ds::make_local_dir_path(destination_dir_path,
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
        ds::make_root_dir_path(root_dir_prefix) + "/" + k_datastore_mark_file_name)) {
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
        std::stringstream ss;
        ss << " Invalid number of MPI processes (provided " << size << ", " << "expected " << correct_mpi_size << ")";
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str());
      }
    }
    if (!priv_global_and(correct_mpi_size, comm)) {
      return false;
    }

    // ----- Remove directories ----- //
    bool ret = true;
    for (int i = 0; i < size; ++i) {
      if (i == rank) {
        if (metall::detail::utility::file_exist(ds::make_root_dir_path(root_dir_prefix))
            && !metall::detail::utility::remove_file(ds::make_root_dir_path(root_dir_prefix))) {
          logger::out(logger::level::error,
                      __FILE__,
                      __LINE__,
                      "Failed to remove directory: " + ds::make_root_dir_path(root_dir_prefix));
          ret = false;
        }
      }
      priv_mpi_barrier(comm);
    }

    return priv_global_and(ret, comm);
  }

  /// \brief Returns the number of partition of a Metall datastore.
  /// \param root_dir_prefix A root directory path of datastore.
  /// \param comm A MPI communicator.
  /// \return The number of partitions of a Metall datastore.
  static int partitions(const char *root_dir_prefix, const MPI_Comm &comm = MPI_COMM_WORLD) {
    return priv_read_num_partitions(root_dir_prefix, comm);
  }

 private:
  static constexpr const char *k_datastore_mark_file_name = "metall_mpi_datastore";
  static constexpr const char *k_partition_size_file_name = "metall_mpi_adaptor_partition_size";

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  static void priv_setup_root_dir(const std::string &root_dir_prefix, const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);
    const std::string root_dir_path = ds::make_root_dir_path(root_dir_prefix);

    // Make sure the root directory and a file with the same name do not exist
    const auto local_ret = metall::detail::utility::file_exist(root_dir_path);
    if (priv_global_or(local_ret, comm)) {
      if (rank == 0) {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "Root directory (or a file with the same name) already exists: " + root_dir_path);
        ::MPI_Abort(comm, -1);
      }
    }
    priv_mpi_barrier(comm);

    for (int i = 0; i < size; ++i) {
      if (i == rank && !metall::detail::utility::directory_exist(root_dir_path)) {
        if (!metall::detail::utility::create_directory(root_dir_path)) {
          logger::out(logger::level::error, __FILE__, __LINE__, "Failed to create directory: " + root_dir_path);
          ::MPI_Abort(comm, -1);
        }

        const std::string mark_file = root_dir_path + "/" + k_datastore_mark_file_name;
        if (!metall::detail::utility::create_file(mark_file)) {
          logger::out(logger::level::error, __FILE__, __LINE__, "Failed to create file: " + mark_file);
          ::MPI_Abort(comm, -1);
        }

        priv_store_partition_size(root_dir_prefix, comm);
      }
      priv_mpi_barrier(comm);
    }
  }

  static void priv_store_partition_size(const std::string &root_dir_prefix, const MPI_Comm &comm) {
    const int size = priv_mpi_comm_size(comm);
    const std::string path = ds::make_root_dir_path(root_dir_prefix) + "/" + k_partition_size_file_name;

    std::ofstream ofs(path);
    if (!ofs) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to create a file: " + path);
      ::MPI_Abort(comm, -1);
    }
    ofs << size;
    if (!ofs) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to write data to a file: " + path);
      ::MPI_Abort(comm, -1);
    }
    ofs.close();
  }

  static int priv_read_num_partitions(const std::string &root_dir_prefix, const MPI_Comm &comm) {
    const std::string path = ds::make_root_dir_path(root_dir_prefix) + "/" + k_partition_size_file_name;
    std::ifstream ifs(path);
    if (!ifs) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to open a file: " + path);
      ::MPI_Abort(comm, -1);
    }
    int read_size = -1;
    if (!(ifs >> read_size)) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to read data from: " + path);
      ::MPI_Abort(comm, -1);
    }

    return read_size;
  }

  static void priv_verify_num_partitions(const std::string &root_dir_prefix, const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    if (rank == 0) {
      if (priv_read_num_partitions(root_dir_prefix, comm) != size) {
        logger::out(logger::level::error, __FILE__, __LINE__, "Invalid number of MPI processes");
        ::MPI_Abort(comm, -1);
      }
    }
    priv_mpi_barrier(comm);
  }

  static int priv_mpi_comm_rank(const MPI_Comm &comm) {
    const int rank = mpi::comm_rank(comm);
    if (rank == -1) {
      ::MPI_Abort(comm, -1);
    }
    return rank;
  }

  static int priv_mpi_comm_size(const MPI_Comm &comm) {
    const int size = mpi::comm_size(comm);
    if (size == -1) {
      ::MPI_Abort(comm, -1);
    }
    return size;
  }

  static void priv_mpi_barrier(const MPI_Comm &comm) {
    if (!mpi::barrier(comm)) {
      ::MPI_Abort(comm, -1);
    }
  }

  static bool priv_global_and(const bool local_result, const MPI_Comm &comm) {
    const auto ret = mpi::global_logical_and(local_result, comm);
    if (!ret.first) {
      ::MPI_Abort(comm, -1);
    }
    return ret.second;
  }

  static bool priv_global_or(const bool local_result, const MPI_Comm &comm) {
    const auto ret = mpi::global_logical_or(local_result, comm);
    if (!ret.first) {
      ::MPI_Abort(comm, -1);
    }
    return ret.second;
  }

  static int priv_determine_local_root_rank(const MPI_Comm &comm) {
    const int rank = mpi::determine_local_root(comm);
    if (rank == -1) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed at determining a local root rank");
      ::MPI_Abort(comm, -1);
    }
    return rank;
  }

  MPI_Comm m_mpi_comm;
  std::string m_root_dir_prefix;
  std::unique_ptr<manager_type> m_local_metall_manager;
};

} // namespace metall::utility

#endif //METALL_UTILITY_METALL_MPI_ADAPTOR_HPP
