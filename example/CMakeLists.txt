add_metall_executable(simple simple.cpp)

add_metall_executable(vector_of_vectors vector_of_vectors.cpp)

add_metall_executable(string string.cpp)

add_metall_executable(string_map string_map.cpp)

add_metall_executable(complex_map complex_map.cpp)

add_metall_executable(offset_pointer offset_pointer.cpp)

add_metall_executable(snapshot snapshot.cpp)

add_metall_executable(csr_graph csr_graph.cpp)

add_metall_executable(fallback_allocator_adaptor fallback_allocator_adaptor.cpp)

add_metall_executable(datastore_description datastore_description.cpp)

add_metall_executable(object_attribute object_attribute.cpp)

add_metall_executable(object_attribute_api_list object_attribute_api_list.cpp)

add_metall_executable(multilevel_containers multilevel_containers.cpp)

add_metall_executable(adjacency_list_graph adjacency_list_graph.cpp)

add_metall_executable(static_mutex static_mutex.cpp)

add_metall_executable(concurrent_map concurrent_map.cpp)

add_metall_executable(metall_containers metall_containers.cpp)

if (BUILD_C)
    add_c_executable(c_api c_api.c)
    target_link_libraries(c_api PRIVATE metall_c)
endif()

if (MPI_CXX_FOUND)
    add_metall_executable(mpi_create mpi_create.cpp)
    target_link_libraries(mpi_create PRIVATE MPI::MPI_CXX)
    if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(mpi_create PRIVATE rt)
    endif()

    add_metall_executable(mpi_open mpi_open.cpp)
    target_link_libraries(mpi_open PRIVATE MPI::MPI_CXX)
    if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Linux")
        target_link_libraries(mpi_open PRIVATE rt)
    endif()

else()
    message(STATUS "Will skip building the MPI examples")
endif()

add_subdirectory(json)
add_subdirectory(container)