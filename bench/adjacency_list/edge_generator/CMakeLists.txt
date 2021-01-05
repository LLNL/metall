add_executable(generate_rmat_edge_list generate_rmat_edge_list.cpp)
if (OpenMP_CXX_FOUND)
    target_link_libraries(generate_rmat_edge_list PUBLIC OpenMP::OpenMP_CXX)
endif ()