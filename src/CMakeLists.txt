if (BUILD_C)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")
    add_library(metall_c STATIC metall_c.cpp)
    add_library(${PROJECT_NAME}::metall_c ALIAS metall_c)
    install(TARGETS metall_c
            EXPORT ${PROJECT_NAME}_Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if (BUILD_UTILITY)
    add_executable(datastore_ls datastore_ls.cpp)
    install(TARGETS datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    add_executable(mpi_datastore_ls mpi_datastore_ls.cpp)
    install(TARGETS mpi_datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()