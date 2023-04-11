// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_METALL_UTILITY_MPI_HPP
#define METALL_METALL_UTILITY_MPI_HPP

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <mpi.h>

#include <metall/logger.hpp>
#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>

/// \namespace metall::utility::mpi
/// \brief Namespace for MPI utilities
namespace metall::utility::mpi {

inline int comm_rank(const MPI_Comm &comm) {
  int rank;
  if (::MPI_Comm_rank(comm, &rank) != MPI_SUCCESS) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed MPI_Comm_rank");
    return -1;
  }
  return rank;
}

inline int comm_size(const MPI_Comm &comm) {
  int size;
  if (::MPI_Comm_size(comm, &size) != MPI_SUCCESS) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed MPI_Comm_size");
    return -1;
  }
  return size;
}

inline bool barrier(const MPI_Comm &comm) {
  if (::MPI_Barrier(comm) != MPI_SUCCESS) {
    logger::out(logger::level::error, __FILE__, __LINE__, "Failed MPI_Barrier");
    return false;
  }
  return true;
}

/// \brief Performs the logical 'and' operation.
/// \param local_value Local bool value.
/// \param comm MPI communicator.
/// \return A pair of bools (error, global value). 'error' is set to false if
/// there is an error.
inline std::pair<bool, bool> global_logical_and(const bool local_value,
                                                const MPI_Comm &comm) {
  char global_value = 0;
  if (::MPI_Allreduce(&local_value, &global_value, 1, MPI_CHAR, MPI_LAND,
                      comm) != MPI_SUCCESS) {
    return std::make_pair(false, global_value);
  }
  return std::make_pair(true, global_value);
}

/// \brief Performs the logical 'or' operation.
/// \param local_value Local bool value.
/// \param comm MPI communicator.
/// \return A pair of bools (error, global value). 'error' is set to false if
/// there is an error.
inline std::pair<bool, bool> global_logical_or(const bool local_value,
                                               const MPI_Comm &comm) {
  char global_value = 0;
  if (::MPI_Allreduce(&local_value, &global_value, 1, MPI_CHAR, MPI_LOR,
                      comm) != MPI_SUCCESS) {
    return std::make_pair(false, false);
  }
  return std::make_pair(true, global_value);
}

/// \brief Determines a local root rank.
/// \param comm MPI communicator.
/// \return Returns a local rank number. On error, returns -1.
inline int determine_local_root(const MPI_Comm &comm) {
  const char *shm_name = "metall_local_root";
  ::shm_unlink(shm_name);
  barrier(comm);

  const int world_rank = comm_rank(comm);
  const int world_size = comm_size(comm);

  // Blocks except for rank 0
  if (world_rank > 0) {
    if (::MPI_Recv(nullptr, 0, MPI_BYTE, world_rank - 1, 1, MPI_COMM_WORLD,
                   MPI_STATUS_IGNORE) != MPI_SUCCESS) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed MPI_Recv");
      return -1;
    }
  }

  const int shm_size = 4096;
  bool this_rank_created = false;

  int shm_fd = ::shm_open(shm_name, O_RDWR, 0666);
  if (shm_fd == -1) {
    shm_fd = ::shm_open(shm_name, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to open & create a shm file");
      return -1;
    }
    this_rank_created = true;

    if (!metall::mtlldetail::extend_file_size(shm_fd, shm_size, false)) {
      logger::out(logger::level::warning, __FILE__, __LINE__,
                  "Failed to extend a shm file; however, continue work");
    }
  }

  // By now, should be ready & correct size
  void *const ptr =
      metall::mtlldetail::map_file_write_mode(shm_fd, nullptr, shm_size, 0, 0);
  if (!ptr) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to map a shm file");
    return -1;
  }

  auto *p_min_rank_size = static_cast<std::pair<int, int> *>(ptr);
  if (this_rank_created) {
    p_min_rank_size->first = world_rank;
    p_min_rank_size->second = 1;
  } else {
    p_min_rank_size->first = std::min(p_min_rank_size->first, world_rank);
    p_min_rank_size->second++;
  }

  // Notifies rank+1 of completion
  if (world_rank < world_size - 1) {
    if (MPI_Send(nullptr, 0, MPI_BYTE, world_rank + 1, 1, MPI_COMM_WORLD) !=
        MPI_SUCCESS) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed MPI_Send");
      return -1;
    }
  }

  // All ranks have completed.  Each pulls their node loacal's data
  barrier(comm);
  const int local_root_rank = p_min_rank_size->first;

  // Close shared segment
  if (!metall::mtlldetail::munmap(shm_fd, ptr, shm_size, false)) {
    logger::out(logger::level::warning, __FILE__, __LINE__,
                "Failed to unmap the shm file; however, continue work.");
  }
  barrier(comm);
  if (this_rank_created && ::shm_unlink(shm_name) != 0) {
    logger::perror(logger::level::warning, __FILE__, __LINE__,
                   "Failed to remove the shm file; however, continue work.");
  }

  return local_root_rank;
}
}  // namespace metall::utility::mpi
#endif  // METALL_METALL_UTILITY_MPI_HPP
