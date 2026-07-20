cmake_minimum_required(VERSION 3.14)
include(FetchContent)

project(myproject LANGUAGES C CXX)

# Boost header files are required for using Metall.
# Resolve Boost before fetching Metall so the source-tree package can attach the
# same transitive Boost usage requirements to Metall::Metall during configure.
find_package(Boost 1.80)
# Download Boost if it does not exit.
# NOTE: this process may take a long time as Boost is a big repository.
if(NOT Boost_FOUND)
    message(STATUS "Download or search previously downloaded Boost using FetchContent")
    FetchContent_Declare(Boost
        URL https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.bz2)
    FetchContent_MakeAvailable(Boost)
    find_package(Boost 1.80 REQUIRED)
endif()

# Download and setup Metall.
if(NOT BUILD_C)
    # Just install Metall header files and avoid building executables in the repository.
    set(JUST_INSTALL_METALL_HEADER TRUE)
endif()
FetchContent_Declare(
    Metall
    GIT_REPOSITORY https://github.com/LLNL/metall.git
    GIT_TAG master
)
FetchContent_MakeAvailable(Metall)

# ---------------------------------------- #
#  For using Metall CXX API
# ---------------------------------------- #

add_executable(cpp_example ../src/cpp_example.cpp)

# Link Metall
# Although target_link_libraries() is used, no library file (e.g., *.a file) is linked.
# Only include path and transitive usage requirements will be set here.
target_link_libraries(cpp_example PRIVATE Metall::Metall)

# ---------------------------------------- #
# For using Metall C API
# ---------------------------------------- #
if(BUILD_C)
    add_executable(c_example ../src/c_example.c)

    # Link Metall C library (libmetall_c)
    target_link_libraries(c_example PRIVATE Metall::metall_c)
endif()
