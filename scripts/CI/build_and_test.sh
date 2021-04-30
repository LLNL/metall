#!/bin/sh

##############################################################################
# Bash script that builds and tests Metall with many compile time configurations
# 1. Set environmental variables for build
# Set manually:
# export CC=gcc
# export CXX=g++
# export CMAKE_PREFIX_PATH=/path/to/boost:${CMAKE_PREFIX_PATH}
#
# Or, configure environmental variables using spack:
# spack load g++
# spack load boost
#
# 2. Set optional environmental variables for test
# export METALL_TEST_DIR=/tmp
# export METALL_LIMIT_MAKE_PARALLELS=n
# export METALL_BUILD_TYPE="Debug;Release;RelWithDebInfo"
#
# 3. Run this script from the root directory of Metall
# sh ./scripts/CI/build_and_test.sh
##############################################################################

#######################################
# Builds and runs test programs
# Globals:
#   METALL_ROOT_DIR
#   METALL_TEST_DIR
#   METALL_LIMIT_MAKE_PARALLELS (option)
# Arguments:
#   CMake options to pass
# Outputs: STDOUT and STDERR
#######################################
run_build_and_test_core() {
  local BUILD_DIR=./build

  mkdir -p ${BUILD_DIR}
  pushd ${BUILD_DIR}
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
  or_die bash ${METALL_ROOT_DIR}/bench/adjacency_list/test/test.sh -d${METALL_TEST_DIR}
  popd

  # Test 3
  rm -rf ${METALL_TEST_DIR}
  pushd bench/adjacency_list
  or_die bash ${METALL_ROOT_DIR}/bench/adjacency_list/test/test_large.sh -d${METALL_TEST_DIR}
  popd

  # TODO: reflink test and C_API test

  rm -rf ${METALL_TEST_DIR}

  popd
  rm -rf ${BUILD_DIR}
}

#######################################
# Show some system information
# Outputs: STDOUT and STDERR
#######################################
show_system_info() {
  exec_cmd df -h
  exec_cmd df -ih
  exec_cmd free -g
  exec_cmd uname -r
}

#######################################
# main function
# Globals:
#   METALL_BUILD_TYPE (option)
#   METALL_TEST_DIR (option, defined if not given)
#   METALL_ROOT_DIR (defined in this function, readonly)
# Outputs: STDOUT and STDERR
#######################################
main() {
  readonly METALL_ROOT_DIR=${PWD}
  source ${METALL_ROOT_DIR}/scripts/test_utility.sh

  show_system_info

  echo "Build and test on ${HOSTNAME}"

  setup_test_dir
  export METALL_TEST_DIR

  local BUILD_TYPES=(Debug)
  if [[ -n "${METALL_BUILD_TYPE}" ]]; then
    BUILD_TYPES=($(echo $METALL_BUILD_TYPE | tr ";" "\n"))
  fi

  for BUILD_TYPE in "${BUILD_TYPES[@]}"; do
    for DISABLE_FREE_FILE_SPACE in OFF; do
      for DISABLE_SMALL_OBJECT_CACHE in OFF; do
        for FREE_SMALL_OBJECT_SIZE_HINT in 0; do
          run_build_and_test_core -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
            -DDISABLE_FREE_FILE_SPACE=${DISABLE_FREE_FILE_SPACE} \
            -DDISABLE_SMALL_OBJECT_CACHE=${DISABLE_SMALL_OBJECT_CACHE} \
            -DFREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT} \
            -DBUILD_BENCH=ON \
            -DBUILD_TEST=ON \
            -DRUN_LARGE_SCALE_TEST=ON \
            -DBUILD_DOC=OFF \
            -DBUILD_DOC_ONLY=OFF \
            -DBUILD_C=ON \
            -DRUN_BUILD_AND_TEST_WITH_CI=ON \
            -DBUILD_VERIFICATION=OFF \
            -DVERBOSE_SYSTEM_SUPPORT_WARNING=OFF \
            -DLOGGING=OFF
        done
      done
    done
  done
}

main "$@"
