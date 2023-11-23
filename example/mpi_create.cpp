// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/utility/metall_mpi_adaptor.hpp>
#include <metall/utility/filesystem.hpp>

int main(int argc, char **argv) {
  ::MPI_Init(&argc, &argv);
  {
    // This over-write mode fails if the existing file/directory is not Metall
    // datastore or the existing datastore was created by a different number of
    // MPI ranks.
    // To forcibly　remove the existing datastore, one can use the following
    // code.
    // metall::utility::filesystem::remove("/tmp/metall_mpi");
    // ::MPI_Barrier(MPI_COMM_WORLD);
    bool overwrite = true;

    metall::utility::metall_mpi_adaptor mpi_adaptor(
        metall::create_only, "/tmp/metall_mpi", MPI_COMM_WORLD, overwrite);
    auto &metall_manager = mpi_adaptor.get_local_manager();

    auto rank = metall_manager.construct<int>("my-rank")();
    ::MPI_Comm_rank(MPI_COMM_WORLD, rank);  // Stores my rank
  }
  ::MPI_Finalize();

  return 0;
}