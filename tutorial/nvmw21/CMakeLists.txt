cmake_minimum_required(VERSION 3.10)
project(Metall-Tutorial-NVMW21)

# ---------- Compiler ---------- #
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_BUILD_TYPE Debug)

set(CMAKE_CXX_FLAGS "-Wall -pthread")

# Debug
set(CMAKE_CXX_FLAGS_DEBUG "-O0")
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -pg")
endif ()


# Requirements for GCC
if (("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.1)
        message(FATAL_ERROR "GCC version must be at least 8.1")
    endif ()
endif ()


# ---------- Boost ---------- #
find_package(Boost 1.80 REQUIRED)

# ---------- MPI ---------- #
find_package(MPI)

# ---------------------------------------- #
# Targets
# ---------------------------------------- #

function(add_metall_exe name)
    add_executable(${name} ${name}.cpp)
    target_include_directories(${name} PRIVATE ../../include ${Boost_INCLUDE_DIRS})
    target_link_libraries(${name} PRIVATE stdc++fs)
endfunction()

add_metall_exe(t0)
add_metall_exe(t1-1)
add_metall_exe(t1-2)
add_metall_exe(t2-1)
add_metall_exe(t2-2)
add_metall_exe(t3)
add_metall_exe(t4-1)
add_metall_exe(t4-2)
add_metall_exe(t5-1)
add_metall_exe(t5-2_create)
add_metall_exe(t5-2_open)