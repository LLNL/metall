cmake_minimum_required(VERSION 3.14)

project(myproject LANGUAGES C CXX)

# Find Metall package
# To configure the search path, see CMake documents.
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

    # Link Metall C library (libmetall_c)
    target_link_libraries(c_example PRIVATE Metall::metall_c)
endif()
