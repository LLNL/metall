# Find the correct option to link the C++17 <filesystem> library.
# If it is not available, uses own implementation.
function(include_cxx_filesystem_library)

    # Check if C++17 <filesystem> header files are available
    # If not, uses our own implementation.
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(filesystem FILESYSTEM_INCLUDE_FILE)
    if (NOT FILESYSTEM_INCLUDE_FILE)
        message(STATUS "Cannot find the C++17 <filesystem> library. Use own implementation.")
        add_definitions(-DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB)
        return()
    endif ()

    # Find the correct option to link the C++17 <filesystem> library
    # GCC: stdc++fs
    # Clang + macOS >= 10.15: c++fs
    # Clang + macOS < 10.15: uses our own implementation
    # Clang + the other OS: c++fs
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")) # GCC
        link_libraries(stdc++fs)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # Clang or AppleClang
        if (NOT APPLE) # Not macOS
            link_libraries(c++fs)
        else()
            include(get_macos_version)
            get_macos_version() # Get MacOS version
            message(VERBOSE "macOS version ${MACOS_VERSION}")
            if (MACOS_VERSION VERSION_GREATER_EQUAL 10.15) # macOS >= 10.15
                link_libraries(c++fs)
            else () # macOS < 10.15
                message(STATUS "macOS >= 10.15 is required to use the C++17 <filesystem> library. Use own implementation.")
                add_definitions(-DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB)
            endif ()
        endif ()
    endif ()

endfunction()