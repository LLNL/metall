include(setup_omp)

add_metall_executable(run_adj_list_bench_stl run_adj_list_bench_stl.cpp)
setup_omp_target(run_adj_list_bench_stl)

add_metall_executable(run_adj_list_bench_bip run_adj_list_bench_bip.cpp)
setup_omp_target(run_adj_list_bench_bip)

add_metall_executable(run_adj_list_bench_bip_extend run_adj_list_bench_bip_extend.cpp)
setup_omp_target(run_adj_list_bench_bip_extend)

add_metall_executable(run_adj_list_bench_metall run_adj_list_bench_metall.cpp)
setup_omp_target(run_adj_list_bench_metall)

add_metall_executable(run_adj_list_bench_reflink_snapshot run_adj_list_bench_reflink_snapshot.cpp)
setup_omp_target(run_adj_list_bench_reflink_snapshot)

if (MEMKIND_ROOT)
    add_metall_executable(run_adj_list_bench_pmem run_adj_list_bench_pmem.cpp)
    target_include_directories(run_adj_list_bench_pmem PRIVATE ${MEMKIND_ROOT}/include)
    if (UNIX AND NOT APPLE)
        find_library(LIBMEMKIND NAMES memkind PATHS ${MEMKIND_ROOT}/lib)
        if (LIBMEMKIND)
            target_link_libraries(run_adj_list_bench_pmem PRIVATE ${LIBMEMKIND})
        else ()
            message(FATAL_ERROR "Cannot find memkind library")
        endif ()
    endif ()
    setup_omp_target(run_adj_list_bench_pmem)
endif ()

configure_file(run_bench.sh run_bench.sh COPYONLY)

add_subdirectory(edge_generator)
add_subdirectory(test)