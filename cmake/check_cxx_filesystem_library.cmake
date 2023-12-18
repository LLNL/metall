# Checks if the C++17 <filesystem> library is available.
function(check_cxx_filesystem_library)

    set(FOUND_CXX17_FILESYSTEM_LIB FALSE PARENT_SCOPE)
    set(REQUIRE_LIB_STDCXX_FS FALSE PARENT_SCOPE)

    # Check if C++17 <filesystem> header files are available
    # If not, uses our own implementation.
    include(CheckIncludeFileCXX)
    CHECK_INCLUDE_FILE_CXX(filesystem FOUND_FILESYSTEM_HEADER)
    if (NOT FOUND_FILESYSTEM_HEADER)
        message(FATAL_ERROR "Cannot find the C++17 <filesystem> library.")
    endif ()

    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")  # Clang or AppleClang
        if (${CMAKE_HOST_SYSTEM_NAME} MATCHES "Darwin") # macOS
            include(get_macos_version)
            get_macos_version() # Get macOS version
            message(VERBOSE "Detected macOS version ${MACOS_VERSION}")
            if (MACOS_VERSION VERSION_LESS 10.15) # macOS < 10.15
                message(FATAL_ERROR "macOS >= 10.15 is required to use the C++17 <filesystem> library.")
            endif ()
        endif ()
    endif ()
endfunction()