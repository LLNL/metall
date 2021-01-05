add_executable(attributed_object_directory_test attributed_object_directory_test.cpp)
link_gtest_lib(attributed_object_directory_test)
gtest_discover_tests(attributed_object_directory_test)

add_executable(bin_manager_test bin_manager_test.cpp)
link_gtest_lib(bin_manager_test)
gtest_discover_tests(bin_manager_test)

add_executable(bin_directory_test bin_directory_test.cpp)
link_gtest_lib(bin_directory_test)
gtest_discover_tests(bin_directory_test)

add_executable(multilayer_bitset_test multilayer_bitset_test.cpp)
link_gtest_lib(multilayer_bitset_test)
gtest_discover_tests(multilayer_bitset_test)

add_executable(chunk_directory_test chunk_directory_test.cpp)
link_gtest_lib(chunk_directory_test)
gtest_discover_tests(chunk_directory_test)

add_executable(manager_test manager_test.cpp)
link_gtest_lib(manager_test)
gtest_discover_tests(manager_test)

if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    add_executable(snapshot_test snapshot_test.cpp)
    link_gtest_lib(snapshot_test)
    gtest_discover_tests(snapshot_test)
endif()

if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    add_executable(copy_file_test copy_file_test.cpp)
    link_gtest_lib(copy_file_test)
    gtest_discover_tests(copy_file_test)
endif()

find_package(OpenMP)
if (OpenMP_CXX_FOUND)
    add_executable(manager_multithread_test manager_multithread_test.cpp)
    target_link_libraries(manager_multithread_test PUBLIC OpenMP::OpenMP_CXX)
    link_gtest_lib(manager_multithread_test)
    gtest_discover_tests(manager_multithread_test)
else()
    MESSAGE(STATUS "OpenMP is not found. Use single thread in the test")
endif()

add_executable(multimanager_test multimanager_test.cpp)
link_gtest_lib(multimanager_test)
gtest_discover_tests(multimanager_test)

add_executable(multifile_segment_storage_test multifile_segment_storage_test.cpp)
link_gtest_lib(multifile_segment_storage_test)
gtest_discover_tests(multifile_segment_storage_test)

add_executable(object_attribute_accessor_test object_attribute_accessor_test.cpp)
link_gtest_lib(object_attribute_accessor_test)
gtest_discover_tests(object_attribute_accessor_test)