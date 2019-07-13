find_package(OpenMP)
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else ()
    MESSAGE(WARNING "OpenMP is not found. Use single thread in adjacency_list.")
endif ()

add_executable(run_adj_list_bench_stl run_adj_list_bench_stl.cpp)
add_executable(run_adj_list_bench_bip run_adj_list_bench_bip.cpp)
add_executable(run_adj_list_bench_bip_extend run_adj_list_bench_bip_extend.cpp)
add_executable(run_adj_list_bench_metall run_adj_list_bench_metall.cpp)
if (BUILD_NUMA)
    if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
        link_libraries(numa)
    endif()
    add_executable(run_adj_list_bench_metall_numa run_adj_list_bench_metall_numa.cpp)
endif()

# Experimental version
#if (JEMALLOC_ROOT)
#    # set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
#
#    add_executable(run_adj_list_bench_metall_jemalloc run_adj_list_bench_metall_jemalloc.cpp)
#
#    # Include jemalloc headers
#    execute_process(COMMAND ${JEMALLOC_ROOT}/bin/jemalloc-config --includedir
#            OUTPUT_VARIABLE JEMALLOC_INCLUDEDIR)
#    string(STRIP ${JEMALLOC_INCLUDEDIR} JEMALLOC_INCLUDEDIR) # Remove the newline at the end
#    target_include_directories(run_adj_list_bench_metall_jemalloc PUBLIC "${JEMALLOC_INCLUDEDIR}")
#
#    # Link to jemalloc library
#    execute_process(COMMAND ${JEMALLOC_ROOT}/bin/jemalloc-config --libdir
#            OUTPUT_VARIABLE JEMALLOC_LIBDIR)
#    string(STRIP ${JEMALLOC_LIBDIR} JEMALLOC_LIBDIR) # Remove the newline at the end
#    target_link_directories(run_adj_list_bench_metall_jemalloc PUBLIC "${JEMALLOC_LIBDIR}")
#    target_link_libraries(run_adj_list_bench_metall_jemalloc -ljemalloc)
#
#    # Libraries required by jemalloc
#    execute_process(COMMAND ${JEMALLOC_ROOT}/bin/jemalloc-config --libs
#            OUTPUT_VARIABLE JEMALLOC_LIBS)
#    string(STRIP ${JEMALLOC_LIBS} JEMALLOC_LIBS) # Remove the newline at the end
#    target_compile_options(run_adj_list_bench_metall_jemalloc PUBLIC "${JEMALLOC_LIBS}")
#endif ()


add_subdirectory(edge_generator)
add_subdirectory(test)