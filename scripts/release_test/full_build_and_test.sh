#!/bin/sh

##############################################################################
# Bash script that builds and tests Metall with all compile time configurations
# This test would take a few hours at least
#
# 1. Set environmental variables for build
# Set manually:
# export CC=gcc
# export CXX=g++
# export CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/path/to/boost
#
# Or, configure environmental variables using spack:
# spack load g++
# spack load boost
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
# Builds documents
# Globals:
#   METALL_ROOT_DIR
#   METALL_BUILD_DIR
# Outputs: STDOUT and STDERR
#######################################
build_docs() {
  mkdir -p ${METALL_BUILD_DIR}
  cd ${METALL_BUILD_DIR}
  echo "Build and test in ${PWD}"

  # Build
  local CMAKE_FILE_LOCATION=${METALL_ROOT_DIR}
  or_die cmake ${CMAKE_FILE_LOCATION} -DBUILD_DOC=ON
  or_die make build_doc

  cd ../
  rm -rf ${METALL_BUILD_DIR}
}

#######################################
# Builds and runs test programs
# Globals:
#   METALL_ROOT_DIR
#   METALL_TEST_DIR
#   METALL_BUILD_DIR
#   METALL_LIMIT_MAKE_PARALLELS (option)
# Arguments:
#   CMake options to pass
# Outputs: STDOUT and STDERR
#######################################
run_build_and_test_core() {
  mkdir -p ${METALL_BUILD_DIR}
  pushd ${METALL_BUILD_DIR}
  echo "Build and test in ${PWD}"

  # Build
  local CMAKE_OPTIONS="$@"
  local CMAKE_FILE_LOCATION=${METALL_ROOT_DIR}
  or_die cmake ${CMAKE_FILE_LOCATION} ${CMAKE_OPTIONS}
  if [[ -z "${METALL_LIMIT_MAKE_PARALLELS}" ]]; then
    or_die make -j
  else
    or_die make -j${METALL_LIMIT_MAKE_PARALLELS}
  fi

  # Test 1
  rm -rf ${METALL_TEST_DIR}
  or_die ctest --timeout 1000

  # Test 2
  rm -rf ${METALL_TEST_DIR}
  pushd bench/adjacency_list
  or_die bash ./test/test.sh -d${METALL_TEST_DIR}
  popd

  # Test 3
  rm -rf ${METALL_TEST_DIR}
  pushd bench/adjacency_list
  or_die bash ./test/test_large.sh -d${METALL_TEST_DIR}
  popd

  # TODO: reflink test and C_API test

  rm -rf ${METALL_TEST_DIR}
  popd
  rm -rf ${METALL_BUILD_DIR}
}

#######################################
# main function
# Globals:
#   METALL_TEST_DIR (option, defined if not given)
#   METALL_BUILD_DIR (option, defined if not given)
#   METALL_ROOT_DIR (defined in this function, readonly)
# Outputs: STDOUT and STDERR
#######################################
main() {
  readonly METALL_ROOT_DIR=${PWD}
  source ${METALL_ROOT_DIR}/scripts/test_utility.sh

  if [[ -z "${METALL_BUILD_DIR}" ]]; then
    readonly METALL_BUILD_DIR="${METALL_ROOT_DIR}/build_${RANDOM}"
  fi

  # Build documents only
  build_docs

  for BUILD_TYPE in Debug RelWithDebInfo Release; do
    for DISABLE_FREE_FILE_SPACE in ON OFF; do
      for DISABLE_SMALL_OBJECT_CACHE in ON OFF; do
        for FREE_SMALL_OBJECT_SIZE_HINT in 0 8 4096 65536; do
          for LOGGING in ON OFF; do
            run_build_and_test_core -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
              -DDISABLE_FREE_FILE_SPACE=${DISABLE_FREE_FILE_SPACE} \
              -DDISABLE_SMALL_OBJECT_CACHE=${DISABLE_SMALL_OBJECT_CACHE} \
              -DFREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT} \
              -DLOGGING=${LOGGING} \
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
