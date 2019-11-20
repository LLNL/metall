// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall_utility/metall_mpi_adaptor.hpp>

int main(int argc, char **argv) {

  ::MPI_Init(&argc, &argv);
  {
    metall_utility::metall_mpi_adaptor mpi_adaptor(metall::create_only, "/tmp");
    auto &metall_manager = mpi_adaptor.get_local_manager();

    auto rank = metall_manager.construct<int>("my-rank")();
    ::MPI_Comm_rank(MPI_COMM_WORLD, rank);
  }
  ::MPI_Finalize();

  return 0;
}