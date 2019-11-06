add_executable(bitset_test bitset_test.cpp)
target_link_libraries(bitset_test gtest_main)
gtest_discover_tests(bitset_test)

if (NOT RUN_BUILD_AND_TEST_WITH_CI)
    add_executable(free_memory_test free_memory_test.cpp)
    target_link_libraries(free_memory_test gtest_main)
    gtest_discover_tests(free_memory_test)
endif()

add_executable(soft_dirty_test soft_dirty_test.cpp)
target_link_libraries(soft_dirty_test gtest_main)
gtest_discover_tests(soft_dirty_test)