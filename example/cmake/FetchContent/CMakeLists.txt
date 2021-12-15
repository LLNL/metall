cmake_minimum_required(VERSION 3.14)

project(myproject)

# Metall requires C++ 17
set(CMAKE_CXX_STANDARD 17)

# Boost header files are required
find_package(Boost 1.64)

# Download and setup Metall.
include(FetchContent)
FetchContent_Declare(
        metall
        GIT_REPOSITORY https://github.com/LLNL/metall.git
        GIT_TAG master
)
FetchContent_MakeAvailable(metall)


# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #
if (Boost_FOUND)
    find_package(Threads)

    add_executable(cpp_example ../src/cpp_example.cpp)

    # Need Boost header files
    target_include_directories(cpp_example PRIVATE ${Boost_INCLUDE_DIRS})

    # Link Metall
    # Although target_link_libraries() is used, no library file (e.g., *.a file) is linked.
    # Only include path will be set here.
    target_link_libraries(cpp_example PRIVATE Metall)

    # This is required if one uses GCC
    target_link_libraries(cpp_example PRIVATE stdc++fs)

    target_link_libraries(cpp_example PRIVATE Threads::Threads)
endif ()


# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #
if (Boost_FOUND)
    add_executable(c_example ../src/c_example.c)

    # Link Metall C library (libmetall_c)
    target_link_libraries(c_example PRIVATE metall_c)
endif ()