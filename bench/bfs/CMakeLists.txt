find_package(OpenMP)
if (NOT OpenMP_CXX_FOUND)
    MESSAGE(STATUS "OpenMP is not found. Use single thread in BFS")
endif()

add_metall_executable(run_bfs_bench_metall run_bfs_bench_metall.cpp)
if (OpenMP_CXX_FOUND)
    target_link_libraries(run_bfs_bench_metall PRIVATE OpenMP::OpenMP_CXX)
endif ()

add_metall_executable(run_bfs_bench_metall_multiple run_bfs_bench_metall_multiple.cpp)
if (OpenMP_CXX_FOUND)
    target_link_libraries(run_bfs_bench_metall_multiple PRIVATE OpenMP::OpenMP_CXX)
endif ()

add_metall_executable(run_bfs_bench_bip run_bfs_bench_bip.cpp)
if (OpenMP_CXX_FOUND)
    target_link_libraries(run_bfs_bench_bip PRIVATE OpenMP::OpenMP_CXX)
endif ()

configure_file(run_bench.sh run_bench.sh COPYONLY)