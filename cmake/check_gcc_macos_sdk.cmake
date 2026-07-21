include(CheckCXXSourceCompiles)

# gcc cannot compile the mach headers of the macOS 26 SDK: they use the C11
# keyword _Static_assert, which gcc does not accept in C++. This affects
# every C++ file that includes the mach headers, for example the mac backend
# of Boost.Chrono built for the tests and benchmarks.
# The workaround force-includes cmake/gcc_macos_sdk_fix.hpp, which maps
# _Static_assert to static_assert for gcc. It is applied only when a compile
# check shows that the mach headers do not compile without it.
macro(apply_gcc_macos_sdk_workaround_if_needed)
    if (CMAKE_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        check_cxx_source_compiles("#include <mach/message.h>\nint main() { return 0; }\n"
                METALL_MACH_HEADERS_COMPILE)
        if (NOT METALL_MACH_HEADERS_COMPILE)
            set(_metall_gcc_sdk_fix_header "${PROJECT_SOURCE_DIR}/cmake/gcc_macos_sdk_fix.hpp")
            set(CMAKE_REQUIRED_FLAGS "-include ${_metall_gcc_sdk_fix_header}")
            check_cxx_source_compiles("#include <mach/message.h>\nint main() { return 0; }\n"
                    METALL_MACH_HEADERS_COMPILE_WITH_FIX)
            unset(CMAKE_REQUIRED_FLAGS)
            if (METALL_MACH_HEADERS_COMPILE_WITH_FIX)
                message(STATUS "Applying the _Static_assert workaround for gcc with this macOS SDK")
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -include ${_metall_gcc_sdk_fix_header}")
            else ()
                message(WARNING "The mach headers of this macOS SDK do not compile with this gcc, "
                        "and the _Static_assert workaround does not help. The build will likely fail.")
            endif ()
            unset(_metall_gcc_sdk_fix_header)
        endif ()
    endif ()
endmacro()
