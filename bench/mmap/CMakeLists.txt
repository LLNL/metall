if (UNIX AND NOT APPLE AND ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    find_package(OpenMP)
    if (OPENMP_FOUND)
        set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS} -D_GLIBCXX_PARALLEL")
        message(STATUS "run_many_files_mmap_bench run with parallel")
    endif()
endif ()

add_executable(run_many_files_mmap_bench run_many_files_mmap_bench.cpp)
