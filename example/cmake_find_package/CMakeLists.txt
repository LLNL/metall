project(myproject)

# Metall requires C++ 17
set(CMAKE_CXX_STANDARD 17)

# Find Metall package
# To configure the search path, CMAKE_PREFIX_PATH environmental variable can be used.
# See CMake's manual for detail.
find_package(Metall REQUIRED)
message(STATUS "Metall_FOUND = ${Metall_FOUND}")


# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #

# Boost header files are required to use Metall C++ API
find_package(Boost 1.64)

if (Boost_FOUND)
    add_executable(cpp_example cpp_example.cpp)

    # Need Boost header files
    target_include_directories(cpp_example PUBLIC ${Boost_INCLUDE_DIRS})

    # Link Metall
    # Although target_link_libraries() is used, no library file (e.g., *.a file) is linked.
    # Only include path will be set here.
    target_link_libraries(cpp_example Metall::Metall)

    # This is required if one uses GCC
    target_link_libraries(cpp_example stdc++fs)
endif ()


# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #
add_executable(c_example c_example.c)

# Link Metall
target_link_libraries(c_example Metall::Metall)

# Link Metall C library (libmetall_c)
target_link_libraries(c_example Metall::metall_c)