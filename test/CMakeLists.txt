#
# Setup googletest downloaded and built by download_and_build_gtest()
#
macro(setup_built_gtest)
    if ((EXISTS ${CMAKE_CURRENT_BINARY_DIR}/googletest-src) AND (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/googletest-build))
        # Add googletest directly to our build. This defines
        # the gtest and gtest_main targets.
        add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                EXCLUDE_FROM_ALL)
    endif()
endmacro()

#
# Download and unpack googletest at configure time
# (see https://github.com/google/googletest/blob/master/googletest/README.md)
#
macro(download_and_build_gtest)
    configure_file(CMakeListsGTest.txt.in googletest-download/CMakeLists.txt)
    execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" .
            RESULT_VARIABLE failed
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if (failed)
        message(FATAL_ERROR "CMake step for googletest failed: ${result}")
    endif ()

    execute_process(COMMAND ${CMAKE_COMMAND} --build .
            RESULT_VARIABLE failed
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/googletest-download)
    if (failed)
        message(FATAL_ERROR "Build step for googletest failed: ${result}")
    endif ()
endmacro()

#
# Find already existing Goggle Test
#
macro(find_existing_gtest)
    # Try to find an already downloaded one first
    setup_built_gtest()

    if ((TARGET gtest) AND (TARGET gtest_main))
        message(STATUS "Found already downloaded googletest")
    else ()
        # Try to find a system-wide one
        find_package(GTest)
        if (GTEST_FOUND)
            message(STATUS "Found a system-wide googletest")
            message(VERBOSE "GTEST_INCLUDE_DIRS=${GTEST_INCLUDE_DIRS}")
            message(VERBOSE "GTEST_LIBRARIES=${GTEST_LIBRARIES}")
            message(VERBOSE "GTEST_MAIN_LIBRARIES=${GTEST_MAIN_LIBRARIES}")
        else ()
            message(FATAL_ERROR "CMake step for googletest failed: ${result}")
        endif ()
    endif ()
endmacro()

# ------------------------------------------------------------ #
# Set up Google Test
# ------------------------------------------------------------ #
if (SKIP_DOWNLOAD_GTEST)
    find_existing_gtest()
elseif(ONLY_DOWNLOAD_GTEST)
    download_and_build_gtest()
    return()
else ()
    download_and_build_gtest()
    setup_built_gtest()
endif ()
# ------------------------------------------------------------ #

include(GoogleTest)

add_subdirectory(utility)
add_subdirectory(v0)