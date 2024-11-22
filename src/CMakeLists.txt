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

    if (BUILD_SHARED_LIBS)
        message(STATUS "Building metall_c as a shared library")
    else ()
        message(STATUS "Building metall_c as a static library")
    endif ()
    # 'BUILD_SHARED_LIBS' variable is used to determine
    # whether to build a shared library or a static library.
    add_library(metall_c metall_c.cpp)
    get_target_property(PIC_metall_c metall_c POSITION_INDEPENDENT_CODE)
    if (PIC_metall_c)
        message(STATUS "POSITION_INDEPENDENT_CODE for metall_c : ON")
    else ()
        message(STATUS "POSITION_INDEPENDENT_CODE for metall_c : OFF")
    endif ()

    add_library(${PROJECT_NAME}::metall_c ALIAS metall_c)

    target_include_directories(metall_c PUBLIC
            $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)

    common_setup_for_metall_executable(metall_c)

    install(TARGETS metall_c
            EXPORT ${PROJECT_NAME}_Targets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif ()