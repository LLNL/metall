cmake_minimum_required(VERSION 3.14...3.30)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

# -------------------------------------------------------------------------------- #
# CMake policy
# -------------------------------------------------------------------------------- #
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.13")
    cmake_policy(SET CMP0077 NEW)
endif()

# When using the URL download method with the ExternalProject_Add() or
# FetchContent_Declare() commands, sets the timestamps of extracted contents
# to the time of extraction.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif()

# Accept <PACKAGENAME>_ROOT variables for find_package() command.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.27")
    cmake_policy(SET CMP0144 NEW)
endif()

# find_package(Boost) to search for the upstream BoostConfig.cmake.
# Disable CMake's FindBoost module.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif()

# -------------------------------------------------------------------------------- #
# Metall general configuration
# -------------------------------------------------------------------------------- #
project(Metall
        VERSION 0.35
        DESCRIPTION "A persistent memory allocator for data-centric analytics"
        HOMEPAGE_URL "https://github.com/LLNL/metall")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)

# ----- Setting up a INTERFACE library to install header files ----- #
include(GNUInstallDirs)
add_library(${PROJECT_NAME} INTERFACE)
add_library(${PROJECT_NAME}::${PROJECT_NAME} ALIAS ${PROJECT_NAME})
target_include_directories(${PROJECT_NAME}
    INTERFACE $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
# Consumers should be able to link only Metall::Metall and inherit the
# language level and compatibility link requirements from the interface target.
target_compile_features(${PROJECT_NAME} INTERFACE cxx_std_17)
target_link_libraries(${PROJECT_NAME}
    INTERFACE "$<$<AND:$<CXX_COMPILER_ID:GNU>,$<VERSION_LESS:$<CXX_COMPILER_VERSION>,9.1>>:stdc++fs>")
include(metall_target_helpers)

# List of Boost components Metall depends on. Defined here (before the config
# file is generated) so MetallConfig.cmake can embed the same list and fetch a
# matching Boost on its own when an installed consumer doesn't already have one.
set(METALL_BOOST_COMPONENTS json unordered interprocess container property_tree uuid graph)
set(METALL_DEFAULT_BOOST_FETCH_URL
    "https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz")

# Generate MetallTargets.cmake file
install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    DESTINATION ${CMAKE_INSTALL_LIBDIR})
# install MetallTargets.cmake
install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE ${PROJECT_NAME}::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})

# Install header files
install(DIRECTORY ${PROJECT_SOURCE_DIR}/include/metall DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

# -------------------------------------------------------------------------------- #
# Generate and install the package configuration and package version files
# -------------------------------------------------------------------------------- #
include(CMakePackageConfigHelpers)

# Generate config file
configure_package_config_file(
    "${PROJECT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME}
    NO_SET_AND_CHECK_MACRO
    NO_CHECK_REQUIRED_COMPONENTS_MACRO)

# Generate the version file for the config file
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY ExactVersion)

# Install the config and version files
install(FILES
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
    "${PROJECT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/${PROJECT_NAME})

# Make this project to also be used from a build directory
export(EXPORT ${PROJECT_NAME}Targets
    FILE "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Targets.cmake"
)
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
option(BUILD_SHARED_LIBS "Generate shared libraries" OFF)
option(BUILD_EXAMPLE "Build the examples" OFF)
option(BUILD_TEST "Build the test" OFF)
option(RUN_LARGE_SCALE_TEST "Run large scale tests" OFF)
option(RUN_BUILD_AND_TEST_WITH_CI "Perform build and basic test with CI" OFF)
option(BUILD_BENCH "Build the benchmark" OFF)
option(BUILD_VERIFICATION "Build verification programs" OFF)
set(COMPILER_DEFS "" CACHE STRING "A list of Metall compile definitions to be added to all targets")
# Options to specify the location of Boost. Setting multiple of these options will result in an error.
set(BOOST_SOURCE_DIR CACHE STRING "Path to Boost libraries directory already uncompressed. Must be a version that supports CMake.")
set(BOOST_FETCH_URL CACHE STRING "URL or file path to an archived Boost source. Must be a version that supports CMake.")
# We keep this option for using old Boost versions. This option will be removed in the future.
set(BOOST_INCLUDE_ROOT "" CACHE STRING "Path to Boost include directory. Specified value is passed to target_include_directories().")

set(metall_boost_source_options "")
foreach(boost_source_option IN ITEMS BOOST_SOURCE_DIR BOOST_FETCH_URL BOOST_INCLUDE_ROOT)
    if(DEFINED ${boost_source_option} AND NOT "${${boost_source_option}}" STREQUAL "")
        list(APPEND metall_boost_source_options ${boost_source_option})
    endif()
endforeach()

list(LENGTH metall_boost_source_options metall_boost_source_option_count)
if(metall_boost_source_option_count GREATER 1)
    list(JOIN metall_boost_source_options ", " metall_boost_source_options_string)
    message(FATAL_ERROR
        "Specify only one of BOOST_SOURCE_DIR, BOOST_FETCH_URL, or BOOST_INCLUDE_ROOT. "
        "Currently set: ${metall_boost_source_options_string}")
endif()

# ---------- Experimental options ---------- #
set(UMAP_ROOT "" CACHE PATH "UMap installed root directory")
# set(PRIVATEER_ROOT "" CACHE PATH "Privateer installed root directory")
option(USE_PRIVATEER "Build with Privateer" OFF)

option(ONLY_DOWNLOAD_GTEST "Only downloading Google Test" OFF)
option(SKIP_DOWNLOAD_GTEST "Skip downloading Google Test" OFF)
option(BUILD_NUMA "Build programs that require the NUMA policy library (numa.h)" OFF)

# Lightweight dependency discovery so the interface target is usable from
# add_subdirectory()/FetchContent() without forcing a full standalone build.
find_package(Threads QUIET)
if(TARGET Threads::Threads)
    target_link_libraries(${PROJECT_NAME} INTERFACE Threads::Threads)
endif()

if(BOOST_INCLUDE_ROOT)
    # Legacy escape hatch for callers that want to supply Boost headers without
    # relying on Boost's CMake package or imported targets.
    metall_add_build_interface_include_dirs(${PROJECT_NAME} ${BOOST_INCLUDE_ROOT})
else()
    # Prefer a transitive Boost dependency so build-tree consumers do not need
    # to repeat include path setup after linking Metall::Metall. On newer CMake,
    # BUILD_LOCAL_INTERFACE keeps that target-based dependency visible only to
    # same-build consumers and strips it from both export() and install(EXPORT).
    # Older CMake releases fall back to mirroring include paths directly.
    find_package(Boost 1.80 QUIET)
    if(TARGET Boost::headers)
        metall_attach_build_tree_boost_usage(${PROJECT_NAME} Boost::headers)
    elseif(TARGET Boost::boost)
        metall_attach_build_tree_boost_usage(${PROJECT_NAME} Boost::boost)
    else()
        metall_attach_build_tree_boost_usage(${PROJECT_NAME})
    endif()
endif()

foreach(compile_definition IN LISTS COMPILER_DEFS)
    target_compile_definitions(${PROJECT_NAME} INTERFACE ${compile_definition})
endforeach()

# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Only downloading Google Test
# -------------------------------------------------------------------------------- #
if(ONLY_DOWNLOAD_GTEST)
    add_subdirectory(test)
    return()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Exit before building anything
# -------------------------------------------------------------------------------- #
if(INSTALL_HEADER_ONLY)
    message(WARNING "INSTALL_HEADER_ONLY option has been replaced with JUST_INSTALL_METALL_HEADER.")
endif()

if(JUST_INSTALL_METALL_HEADER)
    return()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Exit CMake if there is nothing to build
# -------------------------------------------------------------------------------- #
if(NOT (BUILD_UTILITY OR BUILD_C OR BUILD_EXAMPLE OR BUILD_BENCH OR BUILD_TEST OR BUILD_VERIFICATION OR RUN_BUILD_AND_TEST_WITH_CI OR BUILD_DOC))
    return()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Document (Doxygen)
# -------------------------------------------------------------------------------- #
if(BUILD_DOC)
    include(build_doc)
    build_doc()
endif()
# -------------------------------------------------------------------------------- #

# -------------------------------------------------------------------------------- #
# Set up for building executables
# -------------------------------------------------------------------------------- #

# Requirements for GCC
if(NOT RUN_BUILD_AND_TEST_WITH_CI)
    if(("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
            message(FATAL_ERROR "GCC version must be at least 8.1")
        endif()
    endif()
endif()
# ---------- Metall Macros ---------- #
foreach(compile_definition IN LISTS COMPILER_DEFS)
    message(STATUS "Metall compile definition: ${compile_definition}")
endforeach()


# ---------- CMAKE_BUILD_TYPE ---------- #
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is set as Release")
endif()


# ---------- Threads ---------- #
find_package(Threads REQUIRED)

# ---------- UMap ---------- #
if(UMAP_ROOT)
    if(UNIX AND NOT APPLE)
        find_library(LIBUMAP NAMES umap PATHS ${UMAP_ROOT}/lib)
    endif()
endif()

# ---------- Privateer ---------- #
if (FETCH_PRIVATEER_MAIN AND PRIVATEER_ROOT)
    message(FATAL_ERROR "FETCH_PRIVATEER_MAIN and PRIVATEER_ROOT cannot be set at the same time")
endif ()

if (FETCH_PRIVATEER_MAIN)
    set(CMAKE_GIT_TERMINAL_PROMPT OFF CACHE BOOL
        "Disable interactive git credential prompts during FetchContent downloads" FORCE)
    FetchContent_Declare(
            privateer
        GIT_REPOSITORY https://github.com/llnl/Privateer.git
            GIT_TAG feature/auto_deps)
    FetchContent_MakeAvailable(privateer)

    if (EXISTS ${privateer_SOURCE_DIR}/include)
        set(PRIVATEER_ROOT ${privateer_SOURCE_DIR})
    endif ()

    if (TARGET privateer)
        set(LIBPRIVATEER privateer)
    else ()
        find_library(LIBPRIVATEER NAMES privateer
                PATHS ${privateer_BINARY_DIR} ${privateer_BINARY_DIR}/lib ${privateer_SOURCE_DIR}/lib)
    endif ()
endif ()

if (PRIVATEER_ROOT)
    message(STATUS "Privateer Root is ${PRIVATEER_ROOT}")
    if (NOT LIBPRIVATEER)
        find_library(LIBPRIVATEER NAMES privateer PATHS ${PRIVATEER_ROOT}/lib)
    endif ()
endif ()

# ---------- Boost ---------- #
set(METALL_BOOST_HEADER_INCLUDE_DIRS "")
set(METALL_BOOST_LINK_ITEMS "")
set(METALL_BOOST_LOCAL_TARGET_DEPENDENCIES "")
if(BOOST_INCLUDE_ROOT)
    message(STATUS "Boost include root is ${BOOST_INCLUDE_ROOT}")
else()
    include(fetch_boost)

    # List of boost components Metall depends on
    set(BOOST_COMPS ${METALL_BOOST_COMPONENTS})
    # Link Boost UUID with libatomic fails on LC machines
    option(BOOST_UUID_LINK_LIBATOMIC "Link Boost UUID with libatomic" OFF)

    set(metall_boost_fetch_args URL ${METALL_DEFAULT_BOOST_FETCH_URL})

    if(BOOST_SOURCE_DIR)
        set(metall_boost_fetch_args SOURCE ${BOOST_SOURCE_DIR})
    elseif(BOOST_FETCH_URL)
        set(metall_boost_fetch_args URL ${BOOST_FETCH_URL})
    endif()

    fetch_boost(${metall_boost_fetch_args} COMPONENTS ${BOOST_COMPS})

    # Same-build consumers of Metall::Metall, including metall_c, should see
    # the full Boost component usage requirements without local targets leaking
    # into export() or install(EXPORT) metadata.
    if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.26")
        metall_attach_build_tree_boost_usage(${PROJECT_NAME} ${BOOST_LIBS})
    elseif(TARGET Boost::headers)
        metall_attach_build_tree_boost_usage(${PROJECT_NAME} Boost::headers ${BOOST_LIBS})
    elseif(TARGET Boost::boost)
        metall_attach_build_tree_boost_usage(${PROJECT_NAME} Boost::boost ${BOOST_LIBS})
    else()
        metall_attach_build_tree_boost_usage(${PROJECT_NAME})
    endif()

    # Component targets often carry the real include directories and shared-lib
    # compile definitions, while Boost::headers may only be a thin umbrella.
    # Collect those include directories for export-safe library builds.
    metall_collect_target_interface_property(boost_component_include_dirs INTERFACE_INCLUDE_DIRECTORIES ${BOOST_LIBS})
    list(APPEND METALL_BOOST_HEADER_INCLUDE_DIRS ${boost_component_include_dirs})
    list(REMOVE_DUPLICATES METALL_BOOST_HEADER_INCLUDE_DIRS)

    if(NOT METALL_BOOST_HEADER_INCLUDE_DIRS)
        if(Boost_SOURCE_DIR)
            set(METALL_BOOST_HEADER_INCLUDE_DIRS ${Boost_SOURCE_DIR})
        else()
            set(METALL_BOOST_HEADER_INCLUDE_DIRS ${Boost_INCLUDE_DIRS})
        endif()
    endif()

    # Exported build products such as metall_c cannot directly reference local
    # Boost targets in export metadata. Convert those local targets to linker
    # file paths, then add explicit build dependencies so Boost is still built
    # first inside this tree.
    metall_convert_local_targets_to_link_items(METALL_BOOST_LINK_ITEMS ${BOOST_LIBS})
    metall_collect_local_target_dependencies(METALL_BOOST_LOCAL_TARGET_DEPENDENCIES ${BOOST_LIBS})
endif()

# -------------------------------------------------------------------------------- #
# Add executable functions
# -------------------------------------------------------------------------------- #
include(metall_executable_helpers)

# -------------------------------------------------------------------------------- #
# Build tree
# -------------------------------------------------------------------------------- #
add_subdirectory(src)

if(BUILD_EXAMPLE)
    add_subdirectory(example)
endif()

if(BUILD_BENCH)
    add_subdirectory(bench)
endif()

if(BUILD_TEST)
    enable_testing()
    add_subdirectory(test)
endif()

if(BUILD_VERIFICATION)
    add_subdirectory(verification)
endif()
