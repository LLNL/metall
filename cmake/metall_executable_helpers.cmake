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
  if(PRIVATEER_ROOT)
    target_include_directories(${name} PRIVATE ${PRIVATEER_ROOT}/include)
    if(LIBPRIVATEER)
      # 1) Privateer Dependencies
      FIND_PACKAGE(OpenSSL)
      if(OpenSSL_FOUND)
        target_link_libraries(${name} PRIVATE OpenSSL::SSL)
        target_link_libraries(${name} PRIVATE OpenSSL::Crypto)
      endif()
      target_link_libraries(${name} PRIVATE rt)
      FIND_PACKAGE(OpenMP REQUIRED)
      if(OpenMP_CXX_FOUND)
        target_link_libraries(${name} PRIVATE OpenMP::OpenMP_CXX)
      else()
        message(FATAL_ERROR "OpenMP is required to build Metall with Privateer")
      endif()
      if(ZSTD_ROOT)
        find_library(LIBZSTD NAMES zstd PATHS ${ZSTD_ROOT}/lib)
        target_include_directories(${name} PRIVATE ${ZSTD_ROOT}/lib)
        target_link_libraries(${name} PRIVATE ${LIBZSTD})
        target_compile_definitions(${name} PRIVATE USE_COMPRESSION)
      endif()

      # 2) Link Privateer
      target_link_libraries(${name} PRIVATE ${LIBPRIVATEER})
      target_compile_definitions(${name} PRIVATE METALL_USE_PRIVATEER)
    endif()
  endif()
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
