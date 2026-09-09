#!/bin/bash

##############################################################################
# Bash script that builds and tests Metall with multiple build types (e.g., Debug, Release)
# This script is designed to be used in CI.
#
# 1. Set environmental variables for CMake configuration and build, if needed.
# 2. Run this script from the root directory of Metall
# cd metall # Metall root directory
# bash ./scripts/CI/run_ci_build_and_test.sh
##############################################################################

#######################################
# main function
# This function is required to be called from the root directory of Metall
#
# Used environmental variables:
#   METALL_BUILD_DIR (option)
#   METALL_TEST_DIR (option)
#   METALL_BUILD_TYPES (option)
#     E.g. METALL_BUILD_TYPES="Debug;Release;RelWithDebInfo"
#   METALL_CMAKE_ADDITIONAL_OPTIONS (option)
#     E.g. METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_INCLUDE_ROOT=/path/to/boost"
# Outputs: STDOUT and STDERR
#######################################
main() {
  # METALL_ROOT_DIR is required by run_build_and_test_kernel()
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

  local additional_cmake_options=()
  if [[ -n "${METALL_CMAKE_ADDITIONAL_OPTIONS}" ]]; then
    read -r -a additional_cmake_options <<< "${METALL_CMAKE_ADDITIONAL_OPTIONS}"
  fi

  local BUILD_TYPES=(Debug)
  if [[ -n "${METALL_BUILD_TYPES}" ]]; then
    BUILD_TYPES=($(echo $METALL_BUILD_TYPES | tr ";" "\n"))
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
        -DBUILD_VERIFICATION=OFF \
        "${additional_cmake_options[@]}"
  done
}

main "$@"
