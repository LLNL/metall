add_executable(named_object_directory_test named_object_directory_test.cpp)
target_link_libraries(named_object_directory_test gtest_main)
add_test(NAME named_object_directory_test COMMAND named_object_directory_test)

add_executable(bin_manager_test bin_manager_test.cpp)
target_link_libraries(bin_manager_test gtest_main)
add_test(NAME bin_manager_test COMMAND bin_manager_test)

add_executable(bin_directory_test bin_directory_test.cpp)
target_link_libraries(bin_directory_test gtest_main)
add_test(NAME bin_directory_test COMMAND bin_directory_test)

add_executable(multilayer_bitset_test multilayer_bitset_test.cpp)
target_link_libraries(multilayer_bitset_test gtest_main)
add_test(NAME multilayer_bitset_test COMMAND multilayer_bitset_test)

add_executable(chunk_directory_test chunk_directory_test.cpp)
target_link_libraries(chunk_directory_test gtest_main)
add_test(NAME chunk_directory_test COMMAND chunk_directory_test)

add_executable(manager_test manager_test.cpp)
target_link_libraries(manager_test gtest_main)
add_test(NAME manager_test COMMAND manager_test)

if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    add_executable(snapshot_test snapshot_test.cpp)
    target_link_libraries(snapshot_test gtest_main)
    add_test(NAME snapshot_test COMMAND snapshot_test)
endif()

if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    add_executable(copy_file_test copy_file_test.cpp)
    target_link_libraries(copy_file_test gtest_main)
    add_test(NAME copy_file_test COMMAND copy_file_test)
endif()

find_package(OpenMP)
if (OPENMP_FOUND)
    set (CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
    set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")

    add_executable(manager_multithread_test ../v0/manager_multithread_test.cpp)
    target_link_libraries(manager_multithread_test gtest_main)
    add_test(NAME manager_multithread_test COMMAND manager_multithread_test)
else()
    MESSAGE(WARNING "OpenMP is not found. Use single thread in the test")
endif()

add_executable(multimanager_test multimanager_test.cpp)
target_link_libraries(multimanager_test gtest_main)
add_test(NAME multimanager_test COMMAND multimanager_test)