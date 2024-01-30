add_metall_executable(verify_sparse_copy verify_sparse_copy.cpp)

if (Linux)
    add_metall_executable(verify_sparse_copy_syscalls verify_sparse_copy_syscalls.cpp)
endif ()
