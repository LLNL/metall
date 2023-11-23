#!/bin/sh

##############################################################################
# Bash script that builds and tests Metall with many compile time configurations
#
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
# Metall's CMake configuration step downloads the Boost C++ libraries automatically
# if the library is not found.
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
# main function
# Globals:
#   METALL_BUILD_DIR (option, defined if not given)
#   METALL_TEST_DIR (option, defined if not given)
#   METALL_ROOT_DIR (defined in this function, readonly)
#   METALL_BUILD_TYPE (option)
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

  local BUILD_TYPES=(Debug)
  if [[ -n "${METALL_BUILD_TYPE}" ]]; then
    BUILD_TYPES=($(echo $METALL_BUILD_TYPE | tr ";" "\n"))
  fi

  for BUILD_TYPE in "${BUILD_TYPES[@]}"; do
      run_build_and_test_kernel \
        -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
        -DBUILD_BENCH=ON \
        -DBUILD_TEST=ON \
        -DRUN_LARGE_SCALE_TEST=ON \
        -DBUILD_DOC=OFF \
        -DBUILD_C=ON \
        -DBUILD_UTILITY=ON \
        -DBUILD_EXAMPLE=ON \
        -DRUN_BUILD_AND_TEST_WITH_CI=ON \
        -DBUILD_VERIFICATION=OFF
  done
}

main "$@"
