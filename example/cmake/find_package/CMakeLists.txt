project(myproject LANGUAGES C CXX)

# Metall requires C++ 17
set(CMAKE_CXX_STANDARD 17)

# Find Metall package
# To configure the search path, see CMake documents.
find_package(Metall REQUIRED)
message(STATUS "Metall_FOUND = ${Metall_FOUND}")


# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #

# Boost header files are required to use Metall C++ API
find_package(Boost 1.64)
find_package(Threads)

if (Boost_FOUND AND Threads_FOUND)
    add_executable(cpp_example ../src/cpp_example.cpp)

    # Need Boost header files
    target_include_directories(cpp_example PRIVATE ${Boost_INCLUDE_DIRS})

    # Link Metall
    # Although target_link_libraries() is used, no library file (e.g., *.a file) is linked.
    # Only include path will be set here.
    target_link_libraries(cpp_example PRIVATE Metall::Metall)

    # This is required if one uses GCC
    target_link_libraries(cpp_example PRIVATE stdc++fs)

    target_link_libraries(cpp_example PRIVATE Threads::Threads)
endif ()


# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #

# Not required to do this.
# Just checking if metall_c exists.
find_library(LIB_METALL_C NAMES metall_c)

if (LIB_METALL_C)
    add_executable(c_example ../src/c_example.c)

    # Link Metall C library (libmetall_c)
    target_link_libraries(c_example PRIVATE Metall::metall_c)
endif()