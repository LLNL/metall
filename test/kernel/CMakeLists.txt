add_metall_test_executable(attributed_object_directory_test attributed_object_directory_test.cpp)

add_metall_test_executable(bin_manager_test bin_manager_test.cpp)

add_metall_test_executable(bin_directory_test bin_directory_test.cpp)

add_metall_test_executable(multilayer_bitset_test multilayer_bitset_test.cpp)

add_metall_test_executable(chunk_directory_test chunk_directory_test.cpp)

add_metall_test_executable(object_cache_test object_cache_test.cpp)

add_metall_test_executable(manager_test manager_test.cpp)

add_metall_test_executable(manager_test_single_thread manager_test.cpp)
target_compile_definitions(manager_test_single_thread PRIVATE "METALL_DISABLE_CONCURRENCY")

add_metall_test_executable(snapshot_test snapshot_test.cpp)

add_metall_test_executable(copy_datastore_test copy_datastore_test.cpp)

include(setup_omp)
if (OpenMP_CXX_FOUND)
    add_metall_test_executable(manager_multithread_test manager_multithread_test.cpp)
    setup_omp_target(manager_multithread_test)
else()
    MESSAGE(STATUS "OpenMP is not found. Will not run multi-thread test.")
endif()

add_metall_test_executable(multimanager_test multimanager_test.cpp)

add_metall_test_executable(segment_storage_test segment_storage_test.cpp)

add_metall_test_executable(object_attribute_accessor_test object_attribute_accessor_test.cpp)