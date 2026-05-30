// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <mpi.h>

#include <metall/utility/metall_mpi_adaptor.hpp>
#include <metall/utility/filesystem.hpp>

int main(int argc, char **argv) {
  ::MPI_Init(&argc, &argv);
  {
    int rank;
    ::MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    const auto path_generator_independent = [&rank]() {
      return "/tmp/metall_mpi_" + std::to_string(rank);
    };

    // Create
    {
      metall::utility::metall_mpi_adaptor mpi_adaptor(
          metall::create_only, path_generator_independent(), MPI_COMM_WORLD,
          true);
      auto &metall_manager = mpi_adaptor.get_local_manager();
      auto rank_value = metall_manager.construct<int>("rank_value")();
      *rank_value = rank;
    }

    // Open
    {
      metall::utility::metall_mpi_adaptor mpi_adaptor(
          metall::open_only, path_generator_independent(), MPI_COMM_WORLD);
      auto &metall_manager = mpi_adaptor.get_local_manager();
      auto rank_value = metall_manager.find<int>("rank_value").first;
      if (rank_value) {
        std::cout << "Rank " << rank << " opened value " << *rank_value
                  << std::endl;
      } else {
        std::cerr << "Rank " << rank
                  << " failed to find rank_value in the datastore" << std::endl;
        ::MPI_Abort(MPI_COMM_WORLD, -1);
      }

      // snapshot
      const std::filesystem::path snapshot_path = "/tmp/metall_mpi_snapshot";
      if (!mpi_adaptor.snapshot(snapshot_path, true)) {
        std::cerr << "Rank " << rank << " failed to snapshot to "
                  << snapshot_path << std::endl;
        ::MPI_Abort(MPI_COMM_WORLD, -1);
      }
    }

    // Copy
    {
      const std::filesystem::path copy_path = "/tmp/metall_mpi_copy";
      metall::utility::metall_mpi_adaptor::copy(
          path_generator_independent(), copy_path, MPI_COMM_WORLD, true);

      metall::utility::metall_mpi_adaptor mpi_adaptor(
          metall::open_read_only, copy_path, MPI_COMM_WORLD);
      auto &metall_manager = mpi_adaptor.get_local_manager();
      auto rank_value = metall_manager.find<int>("rank_value").first;
      if (rank_value) {
        std::cout << "Rank " << rank << " opened value " << *rank_value
                  << std::endl;
      } else {
        std::cerr << "Rank " << rank
                  << " failed to find rank_value in the datastore" << std::endl;
        ::MPI_Abort(MPI_COMM_WORLD, -1);
      }
    }

    ::MPI_Barrier(MPI_COMM_WORLD);
    if (rank == 0) {
      std::cout << "Test passed" << std::endl;
    }
  }
  ::MPI_Finalize();

  return 0;
}