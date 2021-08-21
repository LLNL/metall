cmake_minimum_required(VERSION 3.10) # gtest_discover_tests requires >= CMake 3.10
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")

# -------------------------------------------------------------------------------- #
# Configuration for installing header file
# -------------------------------------------------------------------------------- #
install(DIRECTORY include/metall DESTINATION include)
install(DIRECTORY include/metall_container DESTINATION include)
install(DIRECTORY include/metall_utility DESTINATION include)

option(INSTALL_HEADER_ONLY "Only installing header files" OFF)
if (INSTALL_HEADER_ONLY)
    return()
endif ()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Configuration for building test, benchmark, example, etc.
# -------------------------------------------------------------------------------- #
project(Metall VERSION 0.16)
configure_file(MetallConfig.h.in MetallConfig.h)

include_directories(${PROJECT_BINARY_DIR})

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# -------------------------------------------------------------------------------- #
# User configurable options
# -------------------------------------------------------------------------------- #
option(BUILD_DOC "Build API documentation" OFF)
option(BUILD_DOC_ONLY "Only build API documentation" OFF)
option(BUILD_C "Build C examples and libraries" OFF)
option(VERBOSE_SYSTEM_SUPPORT_WARNING "Show compile time warning regarding system support" OFF)
option(DISABLE_FREE_FILE_SPACE "Disable freeing file space" OFF)
option(DISABLE_SMALL_OBJECT_CACHE "Disable small object cache" OFF)
option(BUILD_EXAMPLE "Build the examples" OFF)
option(BUILD_BENCH "Build the benchmark" OFF)
option(BUILD_TEST "Build the test" OFF)
option(RUN_LARGE_SCALE_TEST "Run large scale tests" OFF)
option(RUN_BUILD_AND_TEST_WITH_CI "Perform build and basic test with CI" OFF)
option(BUILD_VERIFICATION "Build verification directory" OFF)
option(LOGGING "Logging" OFF)
option(USE_SORTED_BIN "Use VM space aware algorithm in the bin directory" OFF)

set(DEFAULT_VM_RESERVE_SIZE "0" CACHE STRING
        "Set the default VM reserve size (use the internally defined value if 0 is specified)")
set(MAX_SEGMENT_SIZE "0" CACHE STRING
        "Set the max segment size (use the internally defined value if 0 is specified)")
set(INITIAL_SEGMENT_SIZE "0" CACHE STRING
        "Set the initial segment size (use the internally defined value if 0 is specified)")

# ---------- Experimental options ---------- #
option(USE_ANONYMOUS_NEW_MAP "Use the anonymous map when creating a new map region" OFF)
set(UMAP_ROOT "" CACHE PATH "UMap installed root directory")

option(ONLY_DOWNLOAD_GTEST "Only downloading Google Test" OFF)
option(SKIP_DOWNLOAD_GTEST "Skip downloading Google Test" OFF)
option(BUILD_NUMA "Build programs that require the NUMA policy library (numa.h)" OFF)
set(FREE_SMALL_OBJECT_SIZE_HINT "0" CACHE STRING
        "Try to free the associated pages and file space when objects equal to or larger than that is deallocated")

# -------------------------------------------------------------------------------- #

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is set as Release")
endif ()

# ---------- CPP ---------- #
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_CXX_FLAGS "-w -pthread")
else ()
    set(CMAKE_CXX_FLAGS "-Wall -pthread")
endif ()

# Memo:
# On macOS and FreeBSD libc++ is the default standard library and the -stdlib=libc++ is not required.
# https://libcxx.llvm.org/docs/UsingLibcxx.html
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
        NOT (CMAKE_CXX_COMPILER_ID MATCHES "Darwin" OR CMAKE_CXX_COMPILER_ID MATCHES "FreeBSD"))
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif ()

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
endif ()

# Release
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_CXX_FLAGS_RELEASE "-O3")
else ()
    set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
endif ()

# Release with debug info
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-g3 -Og")
if (Linux)
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} -pg")
endif ()

# ---------- C ---------- #
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_C_FLAGS "-w -pthread")
else ()
    set(CMAKE_C_FLAGS "-Wall -pthread")
endif ()

# Debug
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wextra")
if (Linux)
    set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -pg")
endif ()

# Release
if (RUN_BUILD_AND_TEST_WITH_CI)
    set(CMAKE_C_FLAGS_RELEASE "-O3")
else ()
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
if (FREE_SMALL_OBJECT_SIZE_HINT GREATER 0)
    add_definitions(-DMETALL_FREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT})
    message(STATUS "Try to free space for objects >= ${FREE_SMALL_OBJECT_SIZE_HINT} bytes")
endif ()

if (VERBOSE_SYSTEM_SUPPORT_WARNING)
    add_definitions(-DMETALL_VERBOSE_SYSTEM_SUPPORT_WARNING)
    message(STATUS "Show compile time warning regarding system support")
endif ()

if (DISABLE_FREE_FILE_SPACE)
    add_definitions(-DMETALL_DISABLE_FREE_FILE_SPACE)
    message(STATUS "Disable freeing file space in Metall")
endif ()

if (DISABLE_SMALL_OBJECT_CACHE)
    add_definitions(-DMETALL_DISABLE_OBJECT_CACHE)
    message(STATUS "Disable small object cache")
endif ()

if (DEFAULT_VM_RESERVE_SIZE GREATER 0)
    add_definitions(-DMETALL_DEFAULT_VM_RESERVE_SIZE=${DEFAULT_VM_RESERVE_SIZE})
    message(STATUS "METALL_DEFAULT_VM_RESERVE_SIZE=${DEFAULT_VM_RESERVE_SIZE}")
endif ()

if (MAX_SEGMENT_SIZE GREATER 0)
    add_definitions(-DMETALL_MAX_SEGMENT_SIZE=${MAX_SEGMENT_SIZE})
    message(STATUS "METALL_MAX_SEGMENT_SIZE=${MAX_SEGMENT_SIZE}")
endif ()

if (INITIAL_SEGMENT_SIZE GREATER 0)
    add_definitions(-DMETALL_INITIAL_SEGMENT_SIZE=${INITIAL_SEGMENT_SIZE})
    message(STATUS "METALL_INITIAL_SEGMENT_SIZE=${INITIAL_SEGMENT_SIZE}")
endif ()

if (LOGGING)
    add_definitions(-DMETALL_ENABLE_LOGGING)
    message(STATUS "Enable logging")
endif()

if (USE_SORTED_BIN)
    add_definitions(-DMETALL_USE_SORTED_BIN)
    message(STATUS "Use VM space aware algorithm in the bin directory")
endif()

if (USE_ANONYMOUS_NEW_MAP)
    if (USE_ANONYMOUS_NEW_MAP AND UMAP_ROOT)
        message(FATAL_ERROR "USE_ANONYMOUS_NEW_MAP and UMAP_ROOT options cannot coexist")
    endif()

    add_definitions(-DMETALL_USE_ANONYMOUS_NEW_MAP)
    message(STATUS "Use the anonymous map for new map region")
endif()

# -------------------------------------------------------------------------------- #
# Document (Doxygen)
# -------------------------------------------------------------------------------- #
if (BUILD_DOC)
    include(build_doc)
    build_doc()
    if (BUILD_DOC_ONLY)
        return()
    endif ()
endif ()


# -------------------------------------------------------------------------------- #
# Executables
# -------------------------------------------------------------------------------- #
include_directories(include)

# Requirements for GCC
if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
            message(FATAL_ERROR "GCC version must be at least 8.1")
        endif ()
    endif ()
endif ()


# ---------- Only downloading Google Test ---------- #
if (ONLY_DOWNLOAD_GTEST)
    add_subdirectory(test)
    return()
endif ()


# ---------- filesystem ---------- #
include(include_cxx_filesystem_library)
include_cxx_filesystem_library()

# Xcode 11 Beta Release Notes
# Clang now supports the C++17 <filesystem> library for iOS 13, macOS 10.15, watchOS 6, and tvOS 13. (50988273)
# https://developer.apple.com/documentation/xcode_release_notes/xcode_11_beta_release_notes?language=objc


# ---------- UMap ---------- #
if (UMAP_ROOT)
    include_directories(${UMAP_ROOT}/include)
    if (UNIX AND NOT APPLE)
        find_library(LIBUMAP NAMES umap PATHS ${UMAP_ROOT}/lib)
        if (LIBUMAP)
            link_libraries(${LIBUMAP})
            add_definitions(-DMETALL_USE_UMAP)
        else ()
            message(FATAL_ERROR "Cannot find UMap library")
        endif ()
    endif ()
endif ()


# ---------- Boost ---------- #
# Disable the boost-cmake feature (BoostConfig.cmake or boost-config.cmake) since
# there is a tricky behavior/issue especially in Boost 1.70.0.
set (Boost_NO_BOOST_CMAKE ON)

if (RUN_BUILD_AND_TEST_WITH_CI)
    find_package(Boost)
else ()
    find_package(Boost 1.64)
endif ()

if (Boost_FOUND)

    # Just a sanity check
    if (NOT Boost_INCLUDE_DIRS)
        message(FATAL_ERROR "Something is wrong: Boost_INCLUDE_DIRS is empty although Boost_FOUND is true.")
    endif()
    include_directories(${Boost_INCLUDE_DIRS})

    add_subdirectory(src)

    if (BUILD_EXAMPLE)
        add_subdirectory(example)
    endif()

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
