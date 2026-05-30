// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_METALL_MPI_ADAPTOR_HPP
#define METALL_UTILITY_METALL_MPI_ADAPTOR_HPP

#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>
#include <type_traits>
#include <utility>

#include <metall/metall.hpp>
#include <metall/detail/file.hpp>
#include <metall/utility/mpi.hpp>

namespace metall::utility {

namespace mtlldetail {
/// \brief A utility class that can hold either a concrete path or a generator
/// of a path.
///
/// Example:
/// \code
/// void func(const path_holder &path);
///
/// std::filesystem::path path{"/tmp/data"};
/// func(path);
///
/// auto generator = []() -> std::filesystem::path {
///   return "/tmp/data";
/// };
/// func(generator);
/// \endcode
///
class path_holder {
 public:
  // Construct from an already-available path.
  path_holder(std::filesystem::path p) : m_p(std::move(p)) {}

  // Construct from any type that can initialize a filesystem path.
  template <class P, std::enable_if_t<
                         !std::is_same_v<std::decay_t<P>, path_holder> &&
                             std::is_constructible_v<std::filesystem::path, P>,
                         int> = 0>
  path_holder(P &&p) : m_p(std::forward<P>(p)) {}

  // Construct from any callable that returns a path.
  // Exclude path_holder itself to avoid conflicting with copy/move
  // construction.
  template <class F, std::enable_if_t<
                         !std::is_same_v<std::decay_t<F>, path_holder> &&
                             std::is_invocable_r_v<std::filesystem::path, F &>,
                         int> = 0>
  path_holder(F &&f) : m_p(std::forward<F>(f)()) {}

  // Return the stored path directly, or invoke the stored generator.
  std::filesystem::path get() const { return m_p; }

 private:
  // Hold either a concrete path or a function that creates one.
  std::filesystem::path m_p;
};

}  // namespace mtlldetail

/// \brief A utility class for using Metall with MPI.
/// This class constructs a Metall datastore per MPI rank.
/// Each local Metall datastore is stored under a root directory.
/// A root directory path is generated based on the prefix path specified by the
/// user. There can be different prefix paths for different MPI ranks. This is
/// an experimental implementation.
class metall_mpi_adaptor {
 public:
  /// \brief Metall manager type
  using manager_type = metall::manager;

  /// \brief A utility class that can hold either an actual path or a function
  /// that generates a path. This allows users to use different root directory
  /// paths for different MPI ranks by, for example, passing a lambda function
  /// that generates a path based on the MPI rank.
  using path_holder = mtlldetail::path_holder;

  /// \brief Returns the path of the root directory.
  /// metall_mpi_adaptor may generate the root directory path based on the root
  /// dir prefix specified by the user rather than using it directly. This
  /// function return the actual root dirctory path this adaptor uses to store
  /// Metall datastores.
  /// \param root_dir_prefix A prefix of the root directory path.
  /// \return The root directory path generated from the prefix.
  static std::filesystem::path root_datastore_path(
      const path_holder &root_dir_prefix) {
    return root_dir_prefix.get();
  }

  /// \brief Returns the path of a Metall datastore of an MPI rank.
  /// \param root_dir_prefix Prefix path for the datastore.
  /// \param mpi_rank MPI rank.
  /// \return A path of a Metall datastore.
  static std::filesystem::path local_datastore_path(
      const path_holder &root_dir_prefix, const int mpi_rank) {
    return priv_make_local_datastore_path(root_dir_prefix, mpi_rank);
  }

  /// \brief Copies a Metall datastore to another location.
  /// The behavior of copying a data store that is open without the read-only
  /// mode is undefined.
  /// \param source_dir_path A path to a source datastore.
  /// \param destination_dir_path A path to a destination datastore.
  /// \param comm A MPI communicator.
  /// \param overwrite If true, overwrite an existing datastore.
  /// This mode does not overwrite an existing datastore if it is not Metall
  /// datastore created by the same number of MPI processes.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  static bool copy(const path_holder &source_dir_path,
                   const path_holder &destination_dir_path,
                   const MPI_Comm &comm = MPI_COMM_WORLD,
                   bool overwrite = false) {
    if (!consistent(source_dir_path, comm)) {
      if (priv_mpi_comm_rank(comm) == 0) {
        std::stringstream ss;
        ss << "Source directory is not consistnt (may not have closed properly "
              "or may still be open): "
           << source_dir_path.get();
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      }
      return false;
    }
    priv_setup_root_dir(destination_dir_path, overwrite, comm);
    const int rank = priv_mpi_comm_rank(comm);
    return priv_global_and(
        manager_type::copy(local_datastore_path(source_dir_path, rank),
                           local_datastore_path(destination_dir_path, rank)),
        comm);
  }

  /// \brief Removes Metall datastore.
  /// \param root_dir_prefix A root directory path of datastore.
  /// \param comm A MPI communicator.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  /// If there is no directory with the given name, returns true.
  static bool remove(const path_holder &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    const auto root_dir_path = root_dir_prefix.get();

    if (!metall::mtlldetail::file_exist(root_dir_path)) {
      // As long as the root directory does not exist, we consider it as a
      // success.
      return true;
    }

    // ----- Check if this is a Metall datastore ----- //
    bool metall_dir = true;
    if (!metall::mtlldetail::file_exist(root_dir_path /
                                        k_datastore_mark_file_name)) {
      metall_dir = false;
    }
    if (!priv_global_and(metall_dir, comm)) {
      if (rank == 0) {
        std::string s("This is not a Metall datastore: " +
                      root_dir_path.string());
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      }
      return false;
    }
    if (!priv_verify_num_partitions(root_dir_prefix, comm)) {
      return false;
    }

    // ----- Remove directories ----- //
    bool ret = true;
    for (int i = 0; i < size; ++i) {
      if (i == rank) {
        if (metall::mtlldetail::file_exist(root_dir_path) &&
            !metall::mtlldetail::remove_file(root_dir_path)) {
          std::string s("Failed to remove directory: " +
                        root_dir_path.string());
          logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
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
  static int partitions(const path_holder &root_dir_prefix,
                        const MPI_Comm &comm = MPI_COMM_WORLD) {
    return priv_read_partition_size(root_dir_prefix, comm);
  }

  /// \brief Checks if all local datastores are consistent.
  /// \param root_dir_prefix A root directory path of datastore.
  /// \param comm A MPI communicator.
  /// \return Returns true if all datastores are consistent;
  /// otherwise, returns false.
  static bool consistent(const path_holder &root_dir_prefix,
                         const MPI_Comm &comm = MPI_COMM_WORLD) {
    const int rank = priv_mpi_comm_rank(comm);
    const auto local_path = local_datastore_path(root_dir_prefix, rank);
    const auto ret = manager_type::consistent(local_path);
    return priv_global_and(ret, comm);
  }

  /// \brief Opens an existing Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::open_only_t, const path_holder &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    if (!priv_verify_num_partitions(root_dir_prefix, comm)) {
      ::MPI_Abort(comm, -1);
    }
    m_local_metall_manager = std::make_unique<manager_type>(
        metall::open_only,
        local_datastore_path(m_root_dir_prefix, priv_mpi_comm_rank(comm))
            .c_str());
  }

  /// \brief Opens an existing Metall datastore with the read-only mode.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// \param comm A MPI communicator.
  metall_mpi_adaptor(metall::open_read_only_t,
                     const path_holder &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    if (!priv_verify_num_partitions(root_dir_prefix, comm)) {
      ::MPI_Abort(comm, -1);
    }
    m_local_metall_manager = std::make_unique<manager_type>(
        metall::open_read_only,
        local_datastore_path(m_root_dir_prefix, priv_mpi_comm_rank(comm))
            .c_str());
  }

  /// \brief Creates a new Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// The same name of file or directory must not exist.
  /// \param comm A MPI communicator.
  /// \param overwrite If true, overwrite an existing datastore.
  /// This mode does not overwrite an existing datastore if it is not Metall
  /// datastore created by the same number of MPI processes.
  metall_mpi_adaptor(metall::create_only_t, const path_holder &root_dir_prefix,
                     const MPI_Comm &comm = MPI_COMM_WORLD,
                     bool overwrite = false)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_setup_root_dir(root_dir_prefix, overwrite, comm);
    m_local_metall_manager = std::make_unique<manager_type>(
        metall::create_only,
        local_datastore_path(m_root_dir_prefix, priv_mpi_comm_rank(comm))
            .c_str());
  }

  /// \brief Creates a new Metall datastore.
  /// \param root_dir_prefix A root directory path of a Metall datastore.
  /// The same name of file or directory must not exist.
  /// \param capacity The max capacity of the datastore.
  /// \param comm A MPI communicator.
  /// \param overwrite If true, overwrite an existing datastore.
  /// This mode does not overwrite an existing datastore if it is not Metall
  /// datastore created by the same number of MPI processes.
  metall_mpi_adaptor(metall::create_only_t, const path_holder &root_dir_prefix,
                     const std::size_t capacity,
                     const MPI_Comm &comm = MPI_COMM_WORLD,
                     bool overwrite = false)
      : m_mpi_comm(comm),
        m_root_dir_prefix(root_dir_prefix),
        m_local_metall_manager(nullptr) {
    priv_setup_root_dir(root_dir_prefix, overwrite, comm);
    m_local_metall_manager = std::make_unique<manager_type>(
        metall::create_only,
        local_datastore_path(m_root_dir_prefix, priv_mpi_comm_rank(comm))
            .c_str(),
        capacity);
  }

  /// \brief Destructor that globally synchronizes the close operations of all
  /// sub-Metall datastores.
  ~metall_mpi_adaptor() {
    m_local_metall_manager.reset(nullptr);
    priv_mpi_barrier(m_mpi_comm);
  }

  /// \brief Returns the Metall manager object of the rank.
  /// \return A reference to a Metall manager object.
  manager_type &get_local_manager() { return *m_local_metall_manager; }

  /// \brief Returns the Metall manager object of the rank.
  /// \return A reference to a Metall manager object.
  const manager_type &get_local_manager() const {
    return *m_local_metall_manager;
  }

  /// \brief Returns the root path of a Metall datastore
  /// \return A root path of a Metall datastore.
  std::filesystem::path root_datastore_path() const {
    return metall_mpi_adaptor::root_datastore_path(m_root_dir_prefix);
  }

  /// \brief Returns the path of the local Metall datastore of the rank.
  /// \return A path of the local Metall datastore of the rank.
  std::filesystem::path local_datastore_path() const {
    return metall_mpi_adaptor::local_datastore_path(
        m_root_dir_prefix, priv_mpi_comm_rank(m_mpi_comm));
  }

  /// \brief Take a snapshot of the current Metall datastore to another
  /// location. \param destination_dir_path A path to a destination datastore.
  /// \param overwrite If true, overwrite an existing datastore.
  /// This mode does not overwrite an existing datastore if it is not Metall
  /// datastore created by the same number of MPI processes.
  /// \return Returns true if all processes success;
  /// otherwise, returns false.
  bool snapshot(const path_holder &destination_dir_path,
                bool overwrite = false) {
    priv_setup_root_dir(destination_dir_path, overwrite, m_mpi_comm);
    const int rank = priv_mpi_comm_rank(m_mpi_comm);
    return priv_global_and(
        m_local_metall_manager->snapshot(
            local_datastore_path(destination_dir_path, rank).c_str()),
        m_mpi_comm);
  }

 private:
  static constexpr const char *k_datastore_mark_file_name =
      "metall_mpi_datastore";
  static constexpr const char *k_partition_size_file_name =
      "metall_mpi_adaptor_partition_size";

  /// \brief Makes the data store path of a MPI rank.
  /// \param root_dir A root directory path.
  /// \param rank A MPI rank.
  /// \return A path to a local data store directory.
  /// The path can be passed to, for example, metall::manager to perform
  /// operations.
  static std::filesystem::path priv_make_local_datastore_path(
      const path_holder &root_dir, const int rank) {
    return root_dir.get() /
           std::filesystem::path(std::string("subdir-") + std::to_string(rank));
  }

  static void priv_remove_for_overwrite(const path_holder &root_dir_prefix,
                                        const MPI_Comm &comm) {
    if (!remove(root_dir_prefix, comm)) {
      if (priv_mpi_comm_rank(comm) == 0) {
        std::stringstream ss;
        ss << "Failed to overwrite " << root_dir_prefix.get();
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
        ::MPI_Abort(comm, -1);
      }
    }
  }

  static void priv_setup_root_dir(const path_holder &root_dir_prefix,
                                  bool overwrite, const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);
    const auto root_dir_path = root_datastore_path(root_dir_prefix);

    if (overwrite) {
      priv_remove_for_overwrite(root_dir_prefix, comm);
    }

    // Make sure the root directory and a file with the same name do not exist
    const auto local_ret = metall::mtlldetail::file_exist(root_dir_path);
    if (priv_global_or(local_ret, comm)) {
      if (rank == 0) {
        std::string s(
            "Root directory (or a file with the same name) already exists: " +
            root_dir_path.string());
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
        ::MPI_Abort(comm, -1);
      }
    }
    priv_mpi_barrier(comm);

    for (int i = 0; i < size; ++i) {
      if (i == rank && !metall::mtlldetail::directory_exist(root_dir_path)) {
        if (!metall::mtlldetail::create_directory(root_dir_path)) {
          std::string s("Failed to create directory: " +
                        root_dir_path.string());
          logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
          ::MPI_Abort(comm, -1);
        }

        const auto mark_file = root_dir_path / k_datastore_mark_file_name;
        if (!metall::mtlldetail::create_file(mark_file)) {
          std::string s("Failed to create file: " + mark_file.string());
          logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
          ::MPI_Abort(comm, -1);
        }

        priv_store_partition_size(root_dir_prefix, comm);
      }
      priv_mpi_barrier(comm);
    }
  }

  static void priv_store_partition_size(const path_holder &root_dir_prefix,
                                        const MPI_Comm &comm) {
    const int size = priv_mpi_comm_size(comm);
    const auto path =
        root_datastore_path(root_dir_prefix) / k_partition_size_file_name;

    std::ofstream ofs(path);
    if (!ofs) {
      std::string s("Failed to create a file: " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      ::MPI_Abort(comm, -1);
    }
    ofs << size;
    if (!ofs) {
      std::string s("Failed to write data to a file: " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      ::MPI_Abort(comm, -1);
    }
    ofs.close();
  }

  static int priv_read_partition_size(const path_holder &root_dir_prefix,
                                      const MPI_Comm &comm) {
    const auto path =
        root_datastore_path(root_dir_prefix) / k_partition_size_file_name;
    std::ifstream ifs(path);
    if (!ifs) {
      std::string s("Failed to open a file: " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      ::MPI_Abort(comm, -1);
    }
    int read_size = -1;
    if (!(ifs >> read_size)) {
      std::string s("Failed to read data from: " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      ::MPI_Abort(comm, -1);
    }

    return read_size;
  }

  static bool priv_verify_num_partitions(const path_holder &root_dir_prefix,
                                         const MPI_Comm &comm) {
    const int rank = priv_mpi_comm_rank(comm);
    const int size = priv_mpi_comm_size(comm);

    bool correct_mpi_size = true;
    if (rank == 0) {
      const int read_size = priv_read_partition_size(root_dir_prefix, comm);
      if (read_size != size) {
        correct_mpi_size = false;
        std::stringstream ss;
        ss << "Invalid number of MPI processes (provided " << size << ", "
           << "expected " << read_size << ")";
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      }
    }
    return priv_global_and(correct_mpi_size, comm);
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
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed at determining a local root rank");
      ::MPI_Abort(comm, -1);
    }
    return rank;
  }

  MPI_Comm m_mpi_comm;
  path_holder m_root_dir_prefix;
  std::unique_ptr<manager_type> m_local_metall_manager;
};

}  // namespace metall::utility

#endif  // METALL_UTILITY_METALL_MPI_ADAPTOR_HPP
