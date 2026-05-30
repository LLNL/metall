add_metall_test_executable(bitset_test bitset_test.cpp)

include(setup_mpi)
if(MPI_CXX_FOUND)
  add_metall_executable(metall_mpi_adaptor_test metall_mpi_adaptor_test.cpp)
  setup_mpi_target(metall_mpi_adaptor_test)

  foreach(num_ranks 2 4)
    add_test(
      NAME MPIAdaptorTest_${num_ranks}
      COMMAND ${MPIEXEC_EXECUTABLE}
      ${MPIEXEC_NUMPROC_FLAG} ${num_ranks}
      ${MPIEXEC_PREFLAGS}
      $<TARGET_FILE:metall_mpi_adaptor_test>
      ${MPIEXEC_POSTFLAGS})
    set_tests_properties(MPIAdaptorTest_${num_ranks}
      PROPERTIES PROCESSORS ${num_ranks})
  endforeach()
else()
  message(STATUS "MPI is not found. Will not build metall_mpi_adaptor_test.")
endif()
