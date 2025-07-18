cmake_minimum_required(VERSION 3.14)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(FetchContent)

# -------------------------------------------------------------------------------- #
# CMake policy
# -------------------------------------------------------------------------------- #
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.13")
    cmake_policy(SET CMP0077 NEW)
endif ()

# When using the URL download method with the ExternalProject_Add() or
# FetchContent_Declare() commands, sets the timestamps of extracted contents
# to the time of extraction.
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.24")
    cmake_policy(SET CMP0135 NEW)
endif ()

# Accept <PACKAGENAME>_ROOT variables for find_package() command.
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.27")
    cmake_policy(SET CMP0144 NEW)
endif ()

# find_package(Boost) to search for the upstream BoostConfig.cmake.
# Disable CMake's FindBoost module.
if (${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.30")
    cmake_policy(SET CMP0167 NEW)
endif ()

# -------------------------------------------------------------------------------- #
# Metall general configuration
# -------------------------------------------------------------------------------- #
project(Metall
        VERSION 0.32
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

# ---------- Experimental options ---------- #
set(UMAP_ROOT "" CACHE PATH "UMap installed root directory")
set(PRIVATEER_ROOT "" CACHE PATH "Privateer installed root directory")

option(ONLY_DOWNLOAD_GTEST "Only downloading Google Test" OFF)
option(SKIP_DOWNLOAD_GTEST "Skip downloading Google Test" OFF)
option(BUILD_NUMA "Build programs that require the NUMA policy library (numa.h)" OFF)

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
endif ()

if (JUST_INSTALL_METALL_HEADER)
    return()
endif ()
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
# Set up for building executables
# -------------------------------------------------------------------------------- #

# Requirements for GCC
if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
            message(FATAL_ERROR "GCC version must be at least 8.1")
        endif ()
    endif ()
endif ()

# ---------- Metall Macros ---------- #
foreach (X ${COMPILER_DEFS})
    message(STATUS "Metall compile definition: ${X}")
endforeach ()


# ---------- CMAKE_BUILD_TYPE ---------- #
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message(STATUS "CMAKE_BUILD_TYPE is set as Release")
endif ()


# ---------- Threads ---------- #
find_package(Threads REQUIRED)

# ---------- UMap ---------- #
if (UMAP_ROOT)
    if (UNIX AND NOT APPLE)
        find_library(LIBUMAP NAMES umap PATHS ${UMAP_ROOT}/lib)
    endif ()
endif ()

# ---------- Privateer ---------- #
if (PRIVATEER_ROOT)
    message(STATUS "Privateer Root is ${PRIVATEER_ROOT}")
    find_library(LIBPRIVATEER NAMES privateer PATHS ${PRIVATEER_ROOT}/lib)
endif ()

# ---------- Boost ---------- #
if (BOOST_INCLUDE_ROOT)
    message(STATUS "Boost include root is ${BOOST_INCLUDE_ROOT}")
else ()
    include(fetch_boost)

    # List of boost components Metall depends on
    set(BOOST_COMPS json unordered interprocess container property_tree uuid graph)
    # Link Boost UUID with libatomic fails on LC machines
    option(BOOST_UUID_LINK_LIBATOMIC "Link Boost UUID with libatomic" OFF)

    if (BOOST_SOURCE_DIR)
      fetch_boost(SOURCE ${BOOST_SOURCE_DIR} COMPONENTS ${BOOST_COMPS})
    elseif (BOOST_FETCH_URL)
      fetch_boost(URL ${BOOST_FETCH_URL} COMPONENTS ${BOOST_COMPS})
    else ()
      # Use default Boost version if not specified by the user
      fetch_boost(URL "https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz"
                  COMPONENTS ${BOOST_COMPS})
    endif ()
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
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-pg>)
    endif ()

    # Release
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-DNDEBUG>)

    # Release with debug info
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-Ofast>)
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-g3>)
    if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
        target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-pg>)
    endif ()
endfunction()

function(common_setup_for_metall_executable name)
    target_link_libraries(${name} PRIVATE Threads::Threads)
   if (BOOST_INCLUDE_ROOT)
        target_include_directories(${name} PRIVATE ${BOOST_INCLUDE_ROOT})
    else()
        target_link_libraries(${name} PRIVATE ${BOOST_LIBS})
   endif ()

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
    foreach (X ${COMPILER_DEFS})
        target_compile_definitions(${name} PRIVATE ${X})
    endforeach ()
    # --------------------

    # ----- CXX17 Filesystem Lib----- #
    # GNU compilers prior to 9.1 requires linking with stdc++fs
    if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
            target_link_libraries(${name} PRIVATE stdc++fs)
        endif ()
    endif ()
    # --------------------

    # ----- Umap----- #
    if (UMAP_ROOT)
        target_include_directories(${name} PRIVATE ${UMAP_ROOT}/include)
        if (LIBUMAP)
            target_link_libraries(${name} PRIVATE ${LIBUMAP})
            target_compile_definitions(${name} PRIVATE "METALL_USE_UMAP")
        endif ()
    endif ()
    # --------------------

    # ----- Privateer----- #
    if (PRIVATEER_ROOT)
        target_include_directories(${name} PRIVATE ${PRIVATEER_ROOT}/include)
        if (LIBPRIVATEER)
            # 1) Privateer Dependencies
            FIND_PACKAGE(OpenSSL)
            if (OpenSSL_FOUND)
                target_link_libraries(${name} PRIVATE OpenSSL::SSL)
                target_link_libraries(${name} PRIVATE OpenSSL::Crypto)
            endif ()
            target_link_libraries(${name} PRIVATE rt)
            FIND_PACKAGE(OpenMP REQUIRED)
            if (OpenMP_CXX_FOUND)
                target_link_libraries(${name} PRIVATE OpenMP::OpenMP_CXX)
            else ()
                message(FATAL_ERROR "OpenMP is required to build Metall with Privateer")
            endif ()
            if (ZSTD_ROOT)
                find_library(LIBZSTD NAMES zstd PATHS ${ZSTD_ROOT}/lib)
                target_include_directories(${name} PRIVATE ${ZSTD_ROOT}/lib)
                target_link_libraries(${name} PRIVATE ${LIBZSTD})
                target_compile_definitions(${name} PRIVATE USE_COMPRESSION)
            endif ()

            # 2) Link Privateer
            target_link_libraries(${name} PRIVATE ${LIBPRIVATEER})
            target_compile_definitions(${name} PRIVATE METALL_USE_PRIVATEER)
        endif ()
    endif ()
    # --------------------
endfunction()

function(add_metall_executable name source)
    set(ADDED_METALL_EXE FALSE PARENT_SCOPE) # Tell the caller if an executable is added w/o issue

    add_executable(${name} ${source})
    target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    common_setup_for_metall_executable(${name})

    set(ADDED_METALL_EXE TRUE PARENT_SCOPE)
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
