# Find Boost header files
# Input:
#   min_version - minimum required Boost version
#   required - if TRUE, the function will stop the build if Boost headers are not found
#
#  This function first tries to find Boost headers using the find_package() command.
#  If Boost headers are not found, the function tries to find Boost headers manually.
#  In the case, the following variables as hints to find Boost headers, in the following order:
#   BOOST_ROOT  CMake variable, Boost_ROOT CMake variable, BOOST_ROOT environment variable, Boost_ROOT environment variable
#
# Output:
#   Boost_FOUND - TRUE if Boost headers are found
#   Boost_INCLUDE_DIRS - Boost header files directory
#   Boost_VERSION - Boost version
function(find_boost_headers min_version required)
    find_package(Boost ${min_version})
    if (Boost_FOUND)
        set(Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} PARENT_SCOPE)
        set(Boost_FOUND ${Boost_FOUND} PARENT_SCOPE)
        return()
    endif ()

    message(STATUS "Could not find Boost headers using the find_package() command. Trying to find Boost headers manually.")

    # Try to find Boost headers manually using the BOOST_ROOT as a hint
    find_path(Boost_INCLUDE_DIRS boost/version.hpp PATHS ${BOOST_ROOT} ${Boost_ROOT} $ENV{BOOST_ROOT} $ENV{Boost_ROOT} NO_DEFAULT_PATH)
    if (Boost_INCLUDE_DIRS)

        # Extract Boost version value from the boost/version.hpp file
        file(STRINGS ${Boost_INCLUDE_DIRS}/boost/version.hpp _boost_version REGEX "#define BOOST_VERSION[ \t]+[0-9]+")
        string(REGEX REPLACE "#define BOOST_VERSION[ \t]+([0-9]+)" "\\1" Boost_VERSION ${_boost_version})

        # Convert Boost version to the format 'MAJOR.MINOR.PATCH'
        # Major version
        math(EXPR Boost_VERSION_MAJOR "${Boost_VERSION} / 100000")
        # Minor version
        math(EXPR Boost_VERSION_MINOR "(${Boost_VERSION} / 100) % 1000")
        # Patch version
        math(EXPR Boost_VERSION_PATCH "${Boost_VERSION} % 100")
        set(Boost_VERSION "${Boost_VERSION_MAJOR}.${Boost_VERSION_MINOR}.${Boost_VERSION_PATCH}")

        if (${Boost_VERSION} VERSION_GREATER_EQUAL ${min_version})
            message(STATUS "Found Boost headers at ${Boost_INCLUDE_DIRS}")
            message(STATUS "Boost version: ${Boost_VERSION}")
            set(Boost_FOUND TRUE PARENT_SCOPE)
            set(Boost_INCLUDE_DIRS ${Boost_INCLUDE_DIRS} PARENT_SCOPE)
            set(Boost_VERSION ${Boost_VERSION} PARENT_SCOPE)
            return()
        else ()
            message(WARNING "Found an old Boost version '${Boost_VERSION}'. Required version is '${min_version}'.")
        endif ()
    endif ()

    # Not found
    set(Boost_FOUND FALSE PARENT_SCOPE)
    if (required)
        message(FATAL_ERROR "Could not find Boost headers")
    endif ()

endfunction()