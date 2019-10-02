add_executable(simple simple.cpp)

add_executable(vector_of_vectors vector_of_vectors.cpp)

add_executable(multilevel_containers multilevel_containers.cpp)

add_executable(string string.cpp)

add_executable(string_map string_map.cpp)

add_executable(offset_pointer offset_pointer.cpp)

add_executable(snapshot snapshot.cpp)

add_executable(csr_graph csr_graph.cpp)

add_executable(adjacency_list_graph adjacency_list_graph.cpp)

if (USE_NUMA_LIB)
    if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
        link_libraries(numa)
        add_definitions(-DMETALL_USE_NUMA_LIB)
    endif()
endif()
add_executable(custom_kernel_allocator custom_kernel_allocator.cpp)

add_executable(c_api c_api.c)
target_link_libraries(c_api metall_c)