add_executable(bitset_test bitset_test.cpp)
target_link_libraries(bitset_test gtest_main)
add_test(NAME bitset_test COMMAND bitset_test)

add_executable(free_memory_test free_memory_test.cpp)
target_link_libraries(free_memory_test gtest_main)
add_test(NAME free_memory_test COMMAND free_memory_test)

add_executable(soft_dirty_test soft_dirty_test.cpp)
target_link_libraries(soft_dirty_test gtest_main)
add_test(NAME soft_dirty_test COMMAND soft_dirty_test)