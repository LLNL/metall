function(add_common_compile_options name)
  # Common
  target_compile_options(${name} PRIVATE -Wall)

  # Debug
  target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Og>)
  target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-g3>)
  target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-Wextra>)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${name} PRIVATE $<$<CONFIG:Debug>:-pg>)
  endif()

  # Release
  target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-Ofast>)
  target_compile_options(${name} PRIVATE $<$<CONFIG:Release>:-DNDEBUG>)

  # Release with debug info
  target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-Ofast>)
  target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-g3>)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_compile_options(${name} PRIVATE $<$<CONFIG:RelWithDebInfo>:-pg>)
  endif()
endfunction()

function(common_setup_for_metall_executable name)
  get_target_property(metall_target_type ${name} TYPE)

  target_link_libraries(${name} PRIVATE Threads::Threads)
  if(BOOST_INCLUDE_ROOT)
    target_include_directories(${name} PRIVATE ${BOOST_INCLUDE_ROOT})
  else()
    if(metall_target_type MATCHES "_LIBRARY$")
      # Libraries that are exported later must not leak local Boost
      # targets into export metadata. Reuse Metall's same-build usage
      # requirements for compilation, but feed the linker only concrete
      # export-safe items for the compiled Boost libraries.
      target_link_libraries(${name} PRIVATE ${PROJECT_NAME})
      target_link_libraries(${name} PRIVATE ${METALL_BOOST_LINK_ITEMS})
      if(METALL_BOOST_LOCAL_TARGET_DEPENDENCIES)
        add_dependencies(${name} ${METALL_BOOST_LOCAL_TARGET_DEPENDENCIES})
      endif()
    else()
      target_link_libraries(${name} PRIVATE ${BOOST_LIBS})
    endif()
  endif()

  # ----- Compile Options ----- #
  add_common_compile_options(${name})

  # Memo:
  # On macOS and FreeBSD libc++ is the default standard library and the -stdlib=libc++ is not required.
  # https://libcxx.llvm.org/docs/UsingLibcxx.html
  if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
    NOT (CMAKE_CXX_COMPILER_ID MATCHES "Darwin" OR CMAKE_CXX_COMPILER_ID MATCHES "FreeBSD"))
    target_compile_options(${name} PRIVATE -stdlib=libc++)
  endif()
  # --------------------

  # ----- Compile Definitions ----- #
  foreach(X ${COMPILER_DEFS})
    target_compile_definitions(${name} PRIVATE ${X})
  endforeach()
  # --------------------

  # ----- CXX17 Filesystem Lib----- #
  # GNU compilers prior to 9.1 requires linking with stdc++fs
  if(("${CMAKE_C_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU"))
    if(CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.1)
      target_link_libraries(${name} PRIVATE stdc++fs)
    endif()
  endif()
  # --------------------

  # ----- Umap----- #
  if(UMAP_ROOT)
    target_include_directories(${name} PRIVATE ${UMAP_ROOT}/include)
    if(LIBUMAP)
      target_link_libraries(${name} PRIVATE ${LIBUMAP})
      target_compile_definitions(${name} PRIVATE "METALL_USE_UMAP")
    endif()
  endif()
  # --------------------

  # ----- Privateer----- #
  set(PRIVATEER_BUILD_TEST OFF CACHE BOOL "Build Privateer tests")
  set(PRIVATEER_USE_SMARTCACHE OFF CACHE BOOL "Build Privateer with SmartCache")
  set(ENABLE_PAGE_EVICTION ON CACHE BOOL "Build Privateer with page eviction")
  set(ENABLE_COMPRESSION ON CACHE BOOL "Build Privateer with compression")
  if (USE_PRIVATEER)

      set(SPDLOG_BUILD_SHARED ON CACHE BOOL "" FORCE)
      FetchContent_Declare(spdlog
      URL https://github.com/gabime/spdlog/archive/refs/tags/v1.14.1.tar.gz)
      # FetchContent_Populate(spdlog)
      FetchContent_MakeAvailable(spdlog)
      # install(TARGETS spdlog EXPORT PrivateerTargets)
      FetchContent_GetProperties(spdlog BINARY_DIR spdlog_BINARY_DIR)
      FetchContent_GetProperties(spdlog SOURCE_DIR spdlog_SOURCE_DIR)

      FetchContent_Declare(
          Privateer
          GIT_REPOSITORY git@github.com:LLNL/Privateer.git
          GIT_TAG feature/auto_deps # Replace with the correct branch or tag
      )
      FetchContent_MakeAvailable(Privateer)
      message(STATUS "Privateer source directory: ${Privateer_SOURCE_DIR}")
      message(STATUS "Privateer binary directory: ${Privateer_BINARY_DIR}")

      

      target_include_directories(${name} PUBLIC ${Privateer_SOURCE_DIR}/include)
      target_link_directories(${name} PUBLIC ${Privateer_BINARY_DIR}/lib)
    
      


      add_dependencies(${name} privateer)
      target_link_libraries(${name} PUBLIC privateer)
      target_compile_definitions(${name} PUBLIC METALL_USE_PRIVATEER)

      target_include_directories(${name} PUBLIC ${spdlog_SOURCE_DIR}/include)
      target_link_libraries(${name} PUBLIC spdlog::spdlog)
      target_compile_definitions(${name} PUBLIC SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE)
  endif ()
  # --------------------
endfunction()

function(add_metall_executable name source)
  set(ADDED_METALL_EXE FALSE PARENT_SCOPE) # Tell the caller if an executable is added w/o issue

  add_executable(${name} ${source})
  target_include_directories(${name} PRIVATE ${PROJECT_SOURCE_DIR}/include)
  common_setup_for_metall_executable(${name})

  set(ADDED_METALL_EXE TRUE PARENT_SCOPE)
endfunction()

function(add_c_executable name source)
  add_executable(${name} ${source})
  add_common_compile_options(${name})
endfunction()
