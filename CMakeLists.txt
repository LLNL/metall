# -------------------------------------------------------------------------------- #
# Configuration for installing header file
# -------------------------------------------------------------------------------- #
install(DIRECTORY include/metall DESTINATION include)
install(DIRECTORY include/metall_container DESTINATION include)
install(DIRECTORY include/metall_utility DESTINATION include)

option(INSTALL_HEADER_ONLY "Only installing header files" OFF)
if (INSTALL_HEADER_ONLY)
    return()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Configuration for building test, benchmark, example, etc.
# -------------------------------------------------------------------------------- #
cmake_minimum_required(VERSION 3.10) # gtest_discover_tests requires >= CMake 3.10
project(Metall VERSION 0.1)

configure_file(MetallConfig.h.in MetallConfig.h)
include_directories(${PROJECT_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# -------------------------------------------------------------------------------- #
# User configurable options
# -------------------------------------------------------------------------------- #
option(BUILD_BENCH "Build the benchmark" ON)
option(BUILD_TEST "Build the test" OFF)
option(RUN_LARGE_SCALE_TEST "Run large scale tests" OFF)
option(BUILD_DOC "Build documentation" OFF)
option(USE_SPACE_AWARE_BIN "Try to fill from lower address" ON)
option(RUN_BUILD_AND_TEST_WITH_CI "Perform build and basic test with CI" OFF)
option(BUILD_VERIFICATION "Build verification directory" OFF)
option(VERBOSE_SYSTEM_SUPPORT_WARNING "Show compile time warning regarding system support" OFF)
option(BUILD_C "Build C examples and libraries" ON)
option(DISABLE_FREE_FILE_SPACE "Disable freeing file space" OFF)
option(DISABLE_SMALL_OBJECT_CACHE "Disable small object cache" OFF)

# ---------- Experimental options ---------- #
option(ONLY_DOWNLOAD_GTEST "Only downloading Google Test" OFF)
option(SKIP_DOWNLOAD_GTEST "Skip downloading Google Test" OFF)
option(BUILD_NUMA "Build programs that require the NUMA policy library (numa.h)" OFF)
option(FREE_SMALL_OBJECT_SIZE_HINT
       "Free the associated pages and backing file space when objects equal to or larger than this size is deallocated."
       0)
# -------------------------------------------------------------------------------- #

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is set as Release")
endif ()

# ---------- CPP ---------- #
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_CXX_FLAGS "-pthread")
else()
    set(CMAKE_CXX_FLAGS "-Wall -pthread")
endif()

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
endif ()

# Release
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
else()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif ()

# Release with debug info
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -Og")
if (Linux)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
endif ()

# ---------- C ---------- #
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_C_FLAGS "-pthread")
else()
    set(CMAKE_C_FLAGS "-Wall -pthread")
endif()

# Debug
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg")
endif ()

# Release
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_C_FLAGS_RELEASE "-O3")
else()
    set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")
endif ()

# Release with debug info
set(CMAKE_C_FLAGS_RELWITHDEBINFO "-g3 -Og")
if (Linux)
    set(CMAKE_C_FLAGS_RELWITHDEBINFO "${CMAKE_C_FLAGS_RELWITHDEBINFO} -pg")
endif ()

# ---------- MPI ---------- #
find_package(MPI)

# ---------- Configure Metall ---------- #
if (USE_SPACE_AWARE_BIN)
    add_definitions(-DMETALL_USE_SPACE_AWARE_BIN)
    message(STATUS "Try to fill from lower address.")
endif ()

if (FREE_SMALL_OBJECT_SIZE_HINT GREATER 0)
    add_definitions(-DMETALL_FREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT})
    message(STATUS "Try to free space for objects >= ${FREE_SMALL_OBJECT_SIZE_HINT} bytes")
endif()

if (VERBOSE_SYSTEM_SUPPORT_WARNING)
    add_definitions(-DMETALL_VERBOSE_SYSTEM_SUPPORT_WARNING)
    message(STATUS "Show compile time warning regarding system support")
endif()

if (DISABLE_FREE_FILE_SPACE)
    add_definitions(-DMETALL_DISABLE_FREE_FILE_SPACE)
    message(STATUS "Disable freeing file space in Metall")
endif()

if (DISABLE_SMALL_OBJECT_CACHE)
    add_definitions(-DMETALL_DISABLE_OBJECT_CACHE)
    message(STATUS "Disable small object cache")
endif()

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
        message(FATAL_ERROR "Can not find Doxygen")
    endif ()
endif ()


# -------------------------------------------------------------------------------- #
# Executables
# -------------------------------------------------------------------------------- #
# Requirements for GCC
if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.0)
            message(FATAL_ERROR "GCC version must be at least 8.0")
        endif ()
    endif ()
endif ()


# ---------- Only downloading Google Test ---------- #
if (ONLY_DOWNLOAD_GTEST)
    add_subdirectory(test)
    return()
endif()


# ---------- filesystem ---------- #
include(CheckIncludeFileCXX)
CHECK_INCLUDE_FILE_CXX(filesystem FILESYSTEM_INCLUDE_FILE)
if (FILESYSTEM_INCLUDE_FILE)
    # GCC
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        link_libraries(stdc++fs)
    endif ()
    # Clang or AppleClang
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        link_libraries(c++fs)
    endif ()
else ()
    message(NOTICE "Cannot find the filesystem library (C++17). Use own implementation.")
endif ()

# Xcode 11 Beta Release Notes
# Clang now supports the C++17 <filesystem> library for iOS 13, macOS 10.15, watchOS 6, and tvOS 13. (50988273)
# https://developer.apple.com/documentation/xcode_release_notes/xcode_11_beta_release_notes?language=objc


# ---------- UMap ---------- #
if (UMAP_ROOT)
    include_directories(${UMAP_ROOT}/include)
    if(UNIX AND NOT APPLE)
        find_library(LIBUMAP NAMES umap PATHS ${UMAP_ROOT}/lib)
        if (LIBUMAP)
            link_libraries(${LIBUMAP})
            add_definitions(-DMETALL_USE_UMAP)
        else()
            message(FATAL_ERROR "Cannot find umap library")
        endif ()
    endif()
endif()


# ---------- Boost ---------- #
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
            add_definitions(-DMETALL_RUN_LARGE_SCALE_TEST)
            message(STATUS "Run large-scale tests")
        endif ()
        enable_testing()
        add_subdirectory(test)
    endif ()

    if (BUILD_VERIFICATION)
        add_subdirectory(verification)
    endif ()

else ()
    message(FATAL_ERROR "Can not find Boost C++ Libraries")
endif ()
