// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/utility/metall_mpi_adaptor.hpp>

int main(int argc, char **argv) {
  ::MPI_Init(&argc, &argv);
  {
    // mpi_adaptor with the create mode fails if the directory exists.
    // This remove function fails if directory exists and created by a different
    // number of MPI ranks.
    metall::utility::metall_mpi_adaptor::remove("/tmp/dir/metall_mpi");

    metall::utility::metall_mpi_adaptor mpi_adaptor(metall::create_only,
                                                    "/tmp/dir/metall_mpi");
    auto &metall_manager = mpi_adaptor.get_local_manager();

    auto rank = metall_manager.construct<int>("my-rank")();
    ::MPI_Comm_rank(MPI_COMM_WORLD, rank);  // Stores my rank
  }
  ::MPI_Finalize();

  return 0;
}
