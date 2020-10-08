find_package(OpenMP)
if (OPENMP_FOUND)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
else ()
    MESSAGE(STATUS "OpenMP is not found. Use single thread in adjacency_list.")
endif ()

#if (USE_NUMA_LIB)
#    if (${CMAKE_SYSTEM_NAME} MATCHES Linux)
#        link_libraries(numa)
#        add_definitions(-DMETALL_USE_NUMA_LIB)
#    endif()
#endif()

add_executable(run_adj_list_bench_stl run_adj_list_bench_stl.cpp)
add_executable(run_adj_list_bench_bip run_adj_list_bench_bip.cpp)
add_executable(run_adj_list_bench_bip_extend run_adj_list_bench_bip_extend.cpp)
add_executable(run_adj_list_bench_metall run_adj_list_bench_metall.cpp)
add_executable(run_adj_list_bench_reflink_snapshot run_adj_list_bench_reflink_snapshot.cpp)

if (MEMKIND_ROOT)
    include_directories(${MEMKIND_ROOT}/include)
    if(UNIX AND NOT APPLE)
        find_library(LIBMEMKIND NAMES memkind PATHS ${MEMKIND_ROOT}/lib)
        if (LIBMEMKIND)
            link_libraries(${LIBMEMKIND})
        else()
            message(FATAL_ERROR "Cannot find memkind library")
        endif ()
    endif()
    add_executable(run_adj_list_bench_pmem run_adj_list_bench_pmem)
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