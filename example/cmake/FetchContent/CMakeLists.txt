cmake_minimum_required(VERSION 3.14)
include(FetchContent)

project(myproject)

# Metall requires C++ 17
set(CMAKE_CXX_STANDARD 17)

# Boost header files are required for using Metall.
find_package(Boost 1.64)
# Download Boost if it does not exit.
# NOTE: this process may take a long time as Boost is a big repository.
if (NOT Boost_FOUND)
    message(STATUS "Download or search previously downloaded Boost using FetchContent")
    FetchContent_Declare(Boost
            URL https://boostorg.jfrog.io/artifactory/main/release/1.77.0/source/boost_1_77_0.tar.bz2)
    FetchContent_GetProperties(Boost)
    if (NOT Boost_POPULATED)
        FetchContent_Populate(Boost)
    endif ()
    set(BOOST_ROOT ${boost_SOURCE_DIR})
    find_package(Boost 1.64)
endif ()

# Download and setup Metall.
if (NOT BUILD_C)
    # Just install Metall header files and avoid building executables in the repository.
    set(JUST_INSTALL_METALL_HEADER TRUE)
endif()
FetchContent_Declare(
        metall
        GIT_REPOSITORY https://github.com/LLNL/metall.git
        GIT_TAG develop
)
FetchContent_MakeAvailable(metall)

# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #
find_package(Threads)

add_executable(cpp_example ../src/cpp_example.cpp)

# Need Boost header files
target_include_directories(cpp_example PRIVATE ${Boost_INCLUDE_DIRS})

# Link Metall
# Although target_link_libraries() is used, no library file (e.g., *.a file) is linked.
# Only include path will be set here.
target_link_libraries(cpp_example PRIVATE Metall)

# This is required if one uses GCC.
target_link_libraries(cpp_example PRIVATE stdc++fs)

target_link_libraries(cpp_example PRIVATE Threads::Threads)

# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #
if (BUILD_C)
    add_executable(c_example ../src/c_example.c)

    # Link Metall C library (libmetall_c)
    target_link_libraries(c_example PRIVATE metall_c)
endif ()