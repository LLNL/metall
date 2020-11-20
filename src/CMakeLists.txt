set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
add_library(metall_c STATIC metall_c.cpp)
install(TARGETS metall_c RUNTIME DESTINATION lib)

add_executable(datastore_ls datastore_ls.cpp)
install(TARGETS datastore_ls RUNTIME DESTINATION bin)

add_executable(mpi_datastore_ls mpi_datastore_ls.cpp)
install(TARGETS mpi_datastore_ls RUNTIME DESTINATION bin)