#!/bin/sh

##############################################################################
# Bash script that builds and tests Metall with all compile time configurations
# This test would take a few hours at least
#
# 1. Set environmental variables for build, if needed
# Set manually:
# export CC=gcc
# export CXX=g++
# export CMAKE_PREFIX_PATH=/path/to/boost:${CMAKE_PREFIX_PATH}
#
# Or, configure environmental variables using spack:
# spack load g++
# spack load boost
#
# Metall's CMake configuration step downloads the Boost C++ libraries automatically
# if the library is not found.
#
# 2. Set optional environmental variables for test
# export METALL_TEST_DIR=/tmp
# export METALL_BUILD_DIR=./build
# export METALL_LIMIT_MAKE_PARALLELS=n
#
# 3. Run this script from the root directory of Metall
# sh ./scripts/release_test/full_build_and_test.sh
##############################################################################

#######################################
# main function
# Globals:
#   METALL_BUILD_DIR (option, defined if not given)
#   METALL_TEST_DIR (option, defined if not given)
#   METALL_ROOT_DIR (defined in this function, readonly)
# Outputs: STDOUT and STDERR
#######################################
main() {
  readonly METALL_ROOT_DIR=${PWD}
  source ${METALL_ROOT_DIR}/scripts/test_kernel.sh
  source ${METALL_ROOT_DIR}/scripts/test_utility.sh

  echo "Build and test on ${HOSTNAME}"
  show_system_info

  if [[ -z "${METALL_BUILD_DIR}" ]]; then
    readonly METALL_BUILD_DIR="${METALL_ROOT_DIR}/build_${RANDOM}"
  fi

  setup_test_dir
  export METALL_TEST_DIR

  # Build documents
  build_docs

  for BUILD_TYPE in Debug RelWithDebInfo Release; do
    for DISABLE_FREE_FILE_SPACE in ON OFF; do
      for DISABLE_SMALL_OBJECT_CACHE in ON OFF; do
        for FREE_SMALL_OBJECT_SIZE_HINT in 0 8 4096 65536; do
          for USE_ANONYMOUS_NEW_MAP in ON OFF; do
            run_build_and_test_kernel \
              -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
              -DDISABLE_FREE_FILE_SPACE=${DISABLE_FREE_FILE_SPACE} \
              -DDISABLE_SMALL_OBJECT_CACHE=${DISABLE_SMALL_OBJECT_CACHE} \
              -DFREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT} \
              -DUSE_ANONYMOUS_NEW_MAP=${USE_ANONYMOUS_NEW_MAP} \
              -DBUILD_BENCH=ON \
              -DBUILD_TEST=ON \
              -DRUN_LARGE_SCALE_TEST=ON \
              -DBUILD_DOC=OFF \
              -DBUILD_C=ON \
              -DBUILD_UTILITY=ON \
              -DBUILD_EXAMPLE=ON \
              -DRUN_BUILD_AND_TEST_WITH_CI=OFF \
              -DBUILD_VERIFICATION=OFF \
              -DVERBOSE_SYSTEM_SUPPORT_WARNING=ON
            done
        done
      done
    done
  done
}

main "$@"
