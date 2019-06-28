cmake_minimum_required(VERSION 3.5)
project(Metall)


# -------------------------------------------------------------------------------- #
# Options
# -------------------------------------------------------------------------------- #
option(BUILD_BENCH "Build the benchmark" ON)
option(BUILD_TEST "Build the test" OFF)
option(RUN_LARGE_SCALE_TEST "Run large scale tests" OFF)
option(BUILD_DOC "Build documentation" OFF)
option(USE_SPACE_AWARE_BIN "Try to fill from lower address" ON)
option(NUMA_AVAILABLE "Build programs that require the NUMA policy library (numa.h)" OFF)
option(RUN_BUILD_AND_TEST_WITH_CI "Perform build and basic test" OFF)

# ---------- Experimental options ---------- #
option(SKIP_GTEST_DOWNLOAD "Skip downloading Google Test" OFF)
option(USE_ANONYMOUS_MAP "Use anonymous mapping instead of file-backed mapping for application's segment data" OFF)
# -------------------------------------------------------------------------------- #

set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("CMAKE_BUILD_TYPE is set as Release")
endif ()

# ---------- CPP ---------- #
# Basic
set(CMAKE_CXX_FLAGS "-Wall -pthread")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
endif()

# Release
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Release with debug info
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -Og")
if (Linux)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
endif()

# CI
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
endif ()

# ---------- C ---------- #
# Basic
set(CMAKE_C_FLAGS "-Wall -pthread")

# Debug
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg")
endif()

# Release
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

# Release with debug info
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g3 -Og")
if (Linux)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg")
endif()

# CI
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_C_FLAGS_RELEASE "-O3")
endif ()

# ---------- Configure Metall ---------- #
if (USE_SPACE_AWARE_BIN)
    add_definitions(-DUSE_SPACE_AWARE_BIN)
    message(WARNING "Try to fill from lower address.")
endif ()

if (USE_ANONYMOUS_MAP)
    add_definitions(-DUSE_ANONYMOUS_MAP)
    message(WARNING "Use anonymous mapping.")
endif ()


# -------------------------------------------------------------------------------- #
# Document (Doxygen)
# -------------------------------------------------------------------------------- #
if (BUILD_DOC)
    find_package(Doxygen)
    if (DOXYGEN_FOUND)
        set(DOXYGEN_IN ${CMAKE_CURRENT_SOURCE_DIR}/docs/Doxyfile.in)
        set(DOXYGEN_OUT ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

        configure_file(${DOXYGEN_IN} ${DOXYGEN_OUT} @ONLY)

        add_custom_target(doc_doxygen ALL
                COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_OUT}
                WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
                COMMENT "Generating API documentation with Doxygen"
                VERBATIM)
    else ()
        message(WARNING "Can not find Doxygen")
    endif ()
endif ()


# -------------------------------------------------------------------------------- #
# Executables
# -------------------------------------------------------------------------------- #
# Requrements for GCC
if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
            message(FATAL_ERROR "GCC version must be at least 8.0")
        endif ()
    endif ()
endif ()

# ---------- filesytem ---------- #
# Required by GCC for the filesystem library
if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    link_libraries(stdc++fs)
endif ()

# Xcode 11 Beta Release Notes
# Clang now supports the C++17 <filesystem> library for iOS 13, macOS 10.15, watchOS 6, and tvOS 13. (50988273)
# https://developer.apple.com/documentation/xcode_release_notes/xcode_11_beta_release_notes?language=objc
## Required by Clang for the filesystem library
#if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "^(Apple)?Clang$")
#    message("Clang compiler ${CMAKE_CXX_COMPILER_ID}")
#endif()

# Requre Boost
if (RUN_BUILD_AND_TEST_WITH_CI)
    find_package(Boost)
else ()
    find_package(Boost 1.64)
endif ()

if (Boost_FOUND)
    include_directories(${Boost_INCLUDE_DIRS})
    include_directories(include)

    add_subdirectory(src lib)
    add_subdirectory(example)

    if (BUILD_BENCH)
        add_subdirectory(bench)
    endif ()

    if (BUILD_TEST)
        if (RUN_LARGE_SCALE_TEST)
            add_definitions(-DRUN_LARGE_SCALE_TEST)
            message("RUN_LARGE_SCALE_TEST is ON")
        endif ()
        enable_testing()
        add_subdirectory(test)
    endif ()

else ()
    message(FATAL_ERROR "Can not find Boost C++ Libraries")
endif ()
