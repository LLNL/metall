add_executable(simple simple)

add_executable(vector_of_vectors vector_of_vectors)

add_executable(multilevel_containers multilevel_containers)

add_executable(string string)

add_executable(string_map string_map)

add_executable(complex_map complex_map)

add_executable(offset_pointer offset_pointer)

add_executable(snapshot snapshot)

add_executable(csr_graph csr_graph)

add_executable(adjacency_list_graph adjacency_list_graph)

if (USE_NUMA_LIB)
    if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
        link_libraries(numa)
        add_definitions(-DMETALL_USE_NUMA_LIB)
    endif()
endif()
add_executable(custom_kernel_allocator custom_kernel_allocator)

if (BUILD_C)
    add_executable(c_api c_api.c)
    target_link_libraries(c_api metall_c)
endif()

if (MPI_CXX_FOUND)
    add_executable(mpi_create mpi_create)
    target_link_libraries(mpi_create PUBLIC MPI::MPI_CXX)

    add_executable(mpi_open mpi_open)
    target_link_libraries(mpi_open PUBLIC MPI::MPI_CXX)
else()
    message(STATUS "Will skip bulding the MPI examples")
endif()