add_executable(simple simple.cpp)

add_executable(vector_of_vectors vector_of_vectors.cpp)

add_executable(string string.cpp)

add_executable(string_map string_map.cpp)

add_executable(offset_pointer offset_pointer.cpp)

add_executable(snapshot snapshot.cpp)

add_executable(c_api c_api.c)
target_link_libraries(c_api metall_c)
