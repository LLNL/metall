cmake_minimum_required(VERSION 3.5)
project(Metall)

# -------------------------------------------------------------------------------- #
# Options
# -------------------------------------------------------------------------------- #
option(BUILD_BENCH "Build the benchmark" ON)
option(BUILD_TEST "Build the test" OFF)
option(RUN_LARGE_SCALE_TEST "Run large scale tests" OFF)
option(BUILD_DOC "Build documentation" ON)

# ----- Experimental options ----- #
option(SKIP_GTEST_DOWNLOAD "Skip downloading Google Test" OFF)
option(USE_ANONYMOUS_MAP "Use anonymous mapping instead of file-backed mapping for application's segment data" OFF)
# -------------------------------------------------------------------------------- #

set(CMAKE_CXX_STANDARD 17)

if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
    message("CMAKE_BUILD_TYPE is set as Release")
endif ()

set(CMAKE_CXX_FLAGS "-Wall -v -pthread")
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g3 -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

set(CMAKE_C_FLAGS "-Wall -v -pthread")
set(CMAKE_C_FLAGS_DEBUG "-O0 -g3 -Wextra")
set(CMAKE_C_FLAGS_RELEASE "-O3 -DNDEBUG")

if (USE_ANONYMOUS_MAP)
    add_definitions(-DUSE_ANONYMOUS_MAP)
    message(WARNING "Use anonymous mapping.")
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
        message(WARNING "Can not find Doxygen")
    endif ()
endif()

# -------------------------------------------------------------------------------- #
# Executables
# -------------------------------------------------------------------------------- #
# Requre GCC
if ((NOT "${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR (NOT "${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    message(FATAL_ERROR "GCC (C/C++) is requred to buid")
endif ()

# Requre Boost
find_package(Boost)
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
