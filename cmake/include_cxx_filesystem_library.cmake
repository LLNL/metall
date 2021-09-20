# Find the correct option to link the C++17 <filesystem> library.
# If it is not available, uses own implementation.
function(include_cxx_filesystem_library)

    set(FOUND_CXX17_FILESYSTEM_LIB TRUE PARENT_SCOPE)
    set(REQUIRE_LIB_STDCXX_FS FALSE PARENT_SCOPE)

    # Check if C++17 <filesystem> header files are available
    # If not, uses our own implementation.
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(filesystem FOUND_FILESYSTEM_HEADER)
    if (NOT FOUND_FILESYSTEM_HEADER)
        message(STATUS "Cannot find the C++17 <filesystem> library.")
        set(FOUND_CXX17_FILESYSTEM_LIB FALSE PARENT_SCOPE)
        return()
    endif ()

    # Find the correct option to link the C++17 <filesystem> library
    # GCC
    #   Any platform: stdc++fs
    # LLVM
    #   Assumes LLVM >= 9.0 and libc++ >= 7.0.
    #   Clang + macOS >= 10.15: nothing special is required to use <filesystem>
    #   Clang + macOS < 10.15: uses our own implementation
    #   Clang + the other OS: nothing special is required to use <filesystem>
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")) # GCC
        set(REQUIRE_LIB_STDCXX_FS TRUE PARENT_SCOPE)
    elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # Clang or AppleClang
        if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin") # macOS
            include(get_macos_version)
            get_macos_version() # Get macOS version
            message(VERBOSE "macOS version ${MACOS_VERSION}")
            if (MACOS_VERSION VERSION_LESS 10.15) # macOS < 10.15
                message(STATUS "macOS >= 10.15 is required to use the C++17 <filesystem> library.")
                set(FOUND_CXX17_FILESYSTEM_LIB FALSE PARENT_SCOPE)
            endif ()
        endif ()
    endif ()

endfunction()