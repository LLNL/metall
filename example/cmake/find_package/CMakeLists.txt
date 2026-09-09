cmake_minimum_required(VERSION 3.14)

project(myproject LANGUAGES C CXX)

# Find Metall package
# To configure the search path, see CMake documents.
# MetallConfig.cmake resolves (and, if needed, fetches) a suitable Boost on
# its own, so no Boost setup is required here.
find_package(Metall REQUIRED)
message(STATUS "Metall_FOUND = ${Metall_FOUND}")


# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #
add_executable(cpp_example ../src/cpp_example.cpp)

# Link Metall.
# The imported target carries the required C++ standard and transitive
# dependencies such as Boost headers and Threads.
target_link_libraries(cpp_example PRIVATE Metall::Metall)


# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #

# Not required to do this.
# Just checking if metall_c exists.
find_library(LIB_METALL_C NAMES metall_c)

if(LIB_METALL_C)
    add_executable(c_example ../src/c_example.c)

    # Link Metall C library (libmetall_c). Use this helper (provided by
    # MetallConfig.cmake) instead of target_link_libraries() directly; it
    # also fixes up build ordering against metall_c's transitive Boost
    # static libraries for parallel builds (make -j).
    metall_target_link_metall_c(c_example)
endif()
