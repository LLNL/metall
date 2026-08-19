cmake_minimum_required(VERSION 3.14)
include(FetchContent)

project(myproject LANGUAGES C CXX)

# Download and setup Metall.
# Metall's own CMakeLists.txt resolves (and, if needed, fetches) a suitable
# Boost on its own, so no separate Boost setup is required here.
if(NOT BUILD_C)
    # Just install Metall header files and avoid building executables in the repository.
    set(JUST_INSTALL_METALL_HEADER TRUE)
endif()
FetchContent_Declare(
    Metall
    GIT_REPOSITORY https://github.com/LLNL/metall.git
    GIT_TAG feature/brush_up_cmake
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
