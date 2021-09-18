if (BUILD_UTILITY)
    add_metall_executable(datastore_ls datastore_ls.cpp)
    install(TARGETS datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

    add_metall_executable(mpi_datastore_ls mpi_datastore_ls.cpp)
    install(TARGETS mpi_datastore_ls RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()

if (BUILD_C)
    if (NOT Boost_FOUND)
        return()
    endif ()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fPIC")

    add_library(metall_c metall_c.cpp)
    add_library(${PROJECT_NAME}::metall_c ALIAS metall_c)

    target_include_directories(metall_c PUBLIC
            $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    common_setup_for_metall_executable(metall_c)

    install(TARGETS metall_c
            EXPORT ${PROJECT_NAME}_Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()