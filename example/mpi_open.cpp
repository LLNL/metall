// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/utility/metall_mpi_adaptor.hpp>

int main(int argc, char **argv) {
  ::MPI_Init(&argc, &argv);
  {
    metall::utility::metall_mpi_adaptor mpi_adaptor(metall::open_only,
                                                    "/tmp/metall_mpi");
    auto &metall_manager = mpi_adaptor.get_local_manager();

    auto stored_rank = metall_manager.find<int>("my-rank").first;

    int rank;
    ::MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    // The line below should print out:
    // "Rank x opened value x"
    // , where x is a number
    std::cout << "Rank " << rank << " opened value " << *stored_rank
              << std::endl;
  }
  ::MPI_Finalize();

  return 0;
}