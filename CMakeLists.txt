cmake_minimum_required(VERSION 3.10)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

# -------------------------------------------------------------------------------- #
# CMake policy
# -------------------------------------------------------------------------------- #
cmake_policy(SET CMP0077 NEW)

# -------------------------------------------------------------------------------- #
# Metall general configuration
# -------------------------------------------------------------------------------- #
project(Metall
        VERSION 0.21
        DESCRIPTION "A persistent memory allocator for data-centric analytics"
        HOMEPAGE_URL "https://github.com/LLNL/metall")

configure_file(MetallConfig.h.in MetallConfig.h)

# ----- Setting up a INTERFACE library to install header files ----- #
include(GNUInstallDirs)
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME}
        INTERFACE $<BUILD_INTERFACE:${${PROJECT_NAME}_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

install(TARGETS ${PROJECT_NAME}
        EXPORT ${PROJECT_NAME}_Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(EXPORT ${PROJECT_NAME}_Targets
        FILE ${PROJECT_NAME}Targets.cmake
        NAMESPACE ${PROJECT_NAME}::
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/metall DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# -------------------------------------------------------------------------------- #
# Generate and install the package configuration and package version files
# -------------------------------------------------------------------------------- #
include(CMakePackageConfigHelpers)

# generate the version file for the config file
write_basic_package_version_file(
        "${PROJECT_NAME}ConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY ExactVersion)

# create config file
configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)

# install config files
install(FILES
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
        "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/${PROJECT_NAME}/cmake)
# -------------------------------------------------------------------------------- #


# -------------------------------------------------------------------------------- #
# Configuration for building test, benchmark, example, etc.
# ----------
# User configurable options
# -------------------------------------------------------------------------------- #
option(JUST_INSTALL_METALL_HEADER "Just install Metall header files (do not build anything)" OFF)
option(BUILD_UTILITY "Build utility programs" OFF)
option(BUILD_DOC "Build API documentation" OFF)
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

# -------------------------------------------------------------------------------- #
# Only downloading Google Test
# -------------------------------------------------------------------------------- #
if (ONLY_DOWNLOAD_GTEST)
    add_subdirectory(test)
    return()
endif ()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Exit before building anything
# -------------------------------------------------------------------------------- #
if (INSTALL_HEADER_ONLY)
    message(WARNING "INSTALL_HEADER_ONLY option has been replaced with JUST_INSTALL_METALL_HEADER.")
endif()

if (JUST_INSTALL_METALL_HEADER)
    return()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Exit CMake if there is nothing to build
# -------------------------------------------------------------------------------- #
if (NOT (BUILD_UTILITY OR BUILD_C OR BUILD_EXAMPLE OR BUILD_BENCH OR BUILD_TEST OR BUILD_VERIFICATION OR RUN_BUILD_AND_TEST_WITH_CI OR BUILD_DOC))
    return()
endif ()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Document (Doxygen)
# -------------------------------------------------------------------------------- #
if (BUILD_DOC)
    include(build_doc)
    build_doc()
endif ()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Executables
# -------------------------------------------------------------------------------- #
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is set as Release")
endif ()

# ---------- MPI ---------- #
find_package(MPI)

# ---------- Configure Metall ---------- #
if (FREE_SMALL_OBJECT_SIZE_HINT GREATER 0)
    list(APPEND METALL_DEFS "METALL_FREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT}")
    message(STATUS "Try to free space for objects >= ${FREE_SMALL_OBJECT_SIZE_HINT} bytes")
endif ()

if (VERBOSE_SYSTEM_SUPPORT_WARNING)
    list(APPEND METALL_DEFS "METALL_VERBOSE_SYSTEM_SUPPORT_WARNING")
    message(STATUS "Show compile time warning regarding system support")
endif ()

if (DISABLE_FREE_FILE_SPACE)
    list(APPEND METALL_DEFS "METALL_DISABLE_FREE_FILE_SPACE")
    message(STATUS "Disable freeing file space in Metall")
endif ()

if (DISABLE_SMALL_OBJECT_CACHE)
    list(APPEND METALL_DEFS "METALL_DISABLE_OBJECT_CACHE")
    message(STATUS "Disable small object cache")
endif ()

if (DEFAULT_VM_RESERVE_SIZE GREATER 0)
    list(APPEND METALL_DEFS "METALL_DEFAULT_VM_RESERVE_SIZE=${DEFAULT_VM_RESERVE_SIZE}")
    message(STATUS "METALL_DEFAULT_VM_RESERVE_SIZE=${DEFAULT_VM_RESERVE_SIZE}")
endif ()

if (MAX_SEGMENT_SIZE GREATER 0)
    list(APPEND METALL_DEFS "METALL_MAX_SEGMENT_SIZE=${MAX_SEGMENT_SIZE}")
    message(STATUS "METALL_MAX_SEGMENT_SIZE=${MAX_SEGMENT_SIZE}")
endif ()

if (INITIAL_SEGMENT_SIZE GREATER 0)
    list(APPEND METALL_DEFS "METALL_INITIAL_SEGMENT_SIZE=${INITIAL_SEGMENT_SIZE}")
    message(STATUS "METALL_INITIAL_SEGMENT_SIZE=${INITIAL_SEGMENT_SIZE}")
endif ()

if (LOGGING)
    list(APPEND METALL_DEFS "METALL_ENABLE_LOGGING")
    message(STATUS "Enable logging")
endif ()

if (USE_SORTED_BIN)
    list(APPEND METALL_DEFS "METALL_USE_SORTED_BIN")
    message(STATUS "Use VM space aware algorithm in the bin directory")
endif ()

if (USE_ANONYMOUS_NEW_MAP)
    if (USE_ANONYMOUS_NEW_MAP AND UMAP_ROOT)
        message(FATAL_ERROR "USE_ANONYMOUS_NEW_MAP and UMAP_ROOT options cannot coexist")
    endif ()

    list(APPEND METALL_DEFS "METALL_USE_ANONYMOUS_NEW_MAP")
    message(STATUS "Use the anonymous map for new map region")
endif ()


# Requirements for GCC
if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
            message(FATAL_ERROR "GCC version must be at least 8.1")
        endif ()
    endif ()
endif ()


# ---------- Threads ---------- #
find_package(Threads REQUIRED)


# ---------- filesystem ---------- #
include(include_cxx_filesystem_library)
include_cxx_filesystem_library()

# Xcode 11 Beta Release Notes
# Clang now supports the C++17 <filesystem> library for iOS 13, macOS 10.15, watchOS 6, and tvOS 13. (50988273)
# https://developer.apple.com/documentation/xcode_release_notes/xcode_11_beta_release_notes?language=objc


# ---------- UMap ---------- #
if (UMAP_ROOT)
    if (UNIX AND NOT APPLE)
        find_library(LIBUMAP NAMES umap PATHS ${UMAP_ROOT}/lib)
    endif ()
endif ()


# ---------- Boost ---------- #
# Disable the boost-cmake feature (BoostConfig.cmake or boost-config.cmake) since
# there is a tricky behavior/issue especially in Boost 1.70.0.
set(Boost_NO_BOOST_CMAKE ON)

find_package(Boost 1.64 QUIET)
if (NOT Boost_FOUND)
    FetchContent_Declare(Boost
            URL https://boostorg.jfrog.io/artifactory/main/release/1.78.0/source/boost_1_78_0.tar.bz2)
    FetchContent_GetProperties(Boost)
    if (NOT Boost_POPULATED)
        FetchContent_Populate(Boost)
    endif ()
    set(BOOST_ROOT ${boost_SOURCE_DIR})
    find_package(Boost 1.64)
endif ()

# -------------------------------------------------------------------------------- #
# Add executable functions
# -------------------------------------------------------------------------------- #
function(add_common_compile_options name)
    # Common
    target_compile_options(${name} PRIVATE -Wall)

    # Debug
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Og>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-g3>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Wextra>)
    if (Linux)
        target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-pg>)
    endif ()

    # Release
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-DNDEBUG>)

    # Release with debug info
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-g3>)
    if (Linux)
        target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-pg>)
    endif ()
endfunction()

function(common_setup_for_metall_executable name)
    target_include_directories(${name} PRIVATE ${Boost_INCLUDE_DIRS})

    target_link_libraries(${name} PRIVATE Threads::Threads)

    # ----- Compile Options ----- #
    add_common_compile_options(${name})

    # Memo:
    # On macOS and FreeBSD libc++ is the default standard library and the -stdlib=libc++ is not required.
    # https://libcxx.llvm.org/docs/UsingLibcxx.html
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
            NOT (CMAKE_CXX_COMPILER_ID MATCHES "Darwin" OR CMAKE_CXX_COMPILER_ID MATCHES "FreeBSD"))
        target_compile_options(${name} PRIVATE -stdlib=libc++)
    endif ()
    # --------------------

    # ----- Compile Definitions ----- #
    foreach(X IN LISTS METALL_DEFS)
        target_compile_definitions(${name} PRIVATE ${X})
    endforeach()
    # --------------------

    # ----- CXX17 Filesystem Lib----- #
    # include_cxx_filesystem_library module must be executed first
    if (FOUND_CXX17_FILESYSTEM_LIB)
        if (REQUIRE_LIB_STDCXX_FS)
            target_link_libraries(${name} PRIVATE stdc++fs)
        endif()
    elseif()
        target_compile_definitions(${name} PRIVATE "METALL_DISABLE_CXX17_FILESYSTEM_LIB")
    endif()
    # --------------------

    # ----- Umap----- #
    if (UMAP_ROOT)
        target_include_directories(${name} PRIVATE ${UMAP_ROOT}/include)
        if (LIBUMAP)
            target_link_libraries(${name} PRIVATE ${LIBUMAP})
            target_compile_definitions(${name} PRIVATE METALL_USE_UMAP)
        endif ()
    endif ()
    # --------------------
endfunction()

function(add_metall_executable name source)
    set(ADDED_METALL_EXE FALSE PARENT_SCOPE) # Tell the caller if an executable is added w/o issue

    if (Boost_FOUND)
        add_executable(${name} ${source})
        target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
        common_setup_for_metall_executable(${name})

        set(ADDED_METALL_EXE TRUE PARENT_SCOPE)
    endif ()
endfunction()

function(add_c_executable name source)
    add_executable(${name} ${source})
    add_common_compile_options(${name})
endfunction()

# -------------------------------------------------------------------------------- #
# Build tree
# -------------------------------------------------------------------------------- #
add_subdirectory(src)

if (BUILD_EXAMPLE)
    add_subdirectory(example)
endif ()

if (BUILD_BENCH)
    add_subdirectory(bench)
endif ()

if (BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif ()

if (BUILD_VERIFICATION)
    add_subdirectory(verification)
endif ()
