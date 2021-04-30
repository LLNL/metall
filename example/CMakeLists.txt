add_executable(simple simple.cpp)

add_executable(vector_of_vectors vector_of_vectors.cpp)

add_executable(string string.cpp)

add_executable(string_map string_map.cpp)

add_executable(complex_map complex_map.cpp)

add_executable(offset_pointer offset_pointer.cpp)

add_executable(snapshot snapshot.cpp)

add_executable(csr_graph csr_graph.cpp)

add_executable(fallback_allocator_adaptor fallback_allocator_adaptor.cpp)

add_executable(datastore_description datastore_description.cpp)

add_executable(object_attribute object_attribute.cpp)

add_executable(object_attribute_api_list object_attribute_api_list.cpp)

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(STATUS "Skip building the examples that use multi-level containers")
else()
    add_executable(multilevel_containers multilevel_containers.cpp)
    add_executable(adjacency_list_graph adjacency_list_graph.cpp)
endif()

add_executable(static_mutex static_mutex.cpp)

add_executable(concurrent_map concurrent_map.cpp)

add_executable(metall_containers metall_containers.cpp)

if (BUILD_C)
    add_executable(c_api c_api.c)
    target_link_libraries(c_api metall_c)
endif()

if (MPI_CXX_FOUND)
    add_executable(mpi_create mpi_create.cpp)
    target_link_libraries(mpi_create PUBLIC MPI::MPI_CXX)
    if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(mpi_create PUBLIC rt)
    endif()

    add_executable(mpi_open mpi_open.cpp)
    target_link_libraries(mpi_open PUBLIC MPI::MPI_CXX)
    if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(mpi_open PUBLIC rt)
    endif()

else()
    message(STATUS "Will skip building the MPI examples")
endif()