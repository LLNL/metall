#
# Finds an already downloaded and built googletest in local (i.e., this build directory) by download_and_build_gtest().
#
macro(find_local_gtest)
    if ((EXISTS ${CMAKE_CURRENT_BINARY_DIR}/googletest-src) AND (EXISTS ${CMAKE_CURRENT_BINARY_DIR}/googletest-build))
        # Add googletest directly to our build. This defines
        # the gtest and gtest_main targets.
        add_subdirectory(${CMAKE_CURRENT_BINARY_DIR}/googletest-src
                ${CMAKE_CURRENT_BINARY_DIR}/googletest-build
                EXCLUDE_FROM_ALL)
        set(FOUND_LOCAL_GTEST ON)
    endif()
endmacro()

#
# Finds a system-wide googletest
#
macro(find_global_gtest)
    # Find a system-wide one
    find_package(GTest REQUIRED)
    message(STATUS "Found a system-wide googletest")
    message(VERBOSE "GTEST_INCLUDE_DIRS=${GTEST_INCLUDE_DIRS}")
    message(VERBOSE "GTEST_LIBRARIES=${GTEST_LIBRARIES}")
    message(VERBOSE "GTEST_MAIN_LIBRARIES=${GTEST_MAIN_LIBRARIES}")
    set(FOUND_GLOBAL_GTEST ON)
endmacro()

#
# Downloads and unpacks googletest
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
# Link googltest library
#
macro(link_gtest_lib target)
    if (DEFINED FOUND_LOCAL_GTEST)
        target_link_libraries(${target} PRIVATE gtest_main)
    elseif (DEFINED FOUND_GLOBAL_GTEST)
        target_link_libraries(${target} PRIVATE GTest::Main)
    endif ()
endmacro()

# ------------------------------------------------------------ #
# Set up googletest
# ------------------------------------------------------------ #
if(ONLY_DOWNLOAD_GTEST)
    download_and_build_gtest()
    return()
elseif (SKIP_DOWNLOAD_GTEST)
    find_local_gtest()
    if (DEFINED FOUND_LOCAL_GTEST)
        message(STATUS "Found already downloaded googletest")
    else ()
        find_global_gtest()
    endif()
else ()
    download_and_build_gtest()
    find_local_gtest()
    if (NOT DEFINED FOUND_LOCAL_GTEST)
        message(FATAL_ERROR "Cannot find the just downloaded googletest")
    endif()
endif ()

# ------------------------------------------------------------ #

include(GoogleTest)

# -------------------------------------------------------------------------------- #
# Add executable functions
# -------------------------------------------------------------------------------- #
function(add_metall_test_executable name source)
    add_metall_executable(${name} ${source})
    if (ADDED_METALL_EXE)
        link_gtest_lib(${name})
        gtest_discover_tests(${name})
        if (RUN_LARGE_SCALE_TEST)
            target_compile_definitions(${name} PRIVATE "METALL_RUN_LARGE_SCALE_TEST")
        endif()
    endif()
endfunction()

add_subdirectory(utility)
add_subdirectory(kernel)
add_subdirectory(container)