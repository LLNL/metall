#!/bin/sh

# Bash script that builds and tests Metall with many compile time configurations
# Assume that this script is called from Metall root directory

METALL_TEST_DIR="/tmp/${RANDOM}"
METALL_ROOT_DIR=${PWD}

or_die () {
    "$@"
    local status=$?
    if [[ $status != 0 ]] ; then
        echo ERROR $status command: $@
        exit $status
    fi
}

# As multiple CI jobs could run on the same machine
# generate an unique test dir for each CI job
setup_test_dir() {
  # CI_JOB_ID is set by Gitlab
  if [[ -z "${CI_JOB_ID}" ]]; then
    echo "Cannot find an environmental variable CI_JOB_ID"
  else
    METALL_TEST_DIR="/tmp/${CI_JOB_ID}"
  fi

  mkdir -p ${METALL_TEST_DIR}
  echo "Store test data to ${METALL_TEST_DIR}"
}

run_buid_and_test_core() {
    export METALL_TEST_DIR

    mkdir -p ./build
    cd build
    echo "Build in ${PWD}"

    # Build
    CMAKE_OPTIONS="$@"
    CMAKE_FILE_LOCATION=${METALL_ROOT_DIR}
    or_die cmake ${CMAKE_FILE_LOCATION} \
                 -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
                 -DBUILD_BENCH=ON -DBUILD_TEST=ON -DRUN_LARGE_SCALE_TEST=ON -DBUILD_DOC=OFF -DRUN_BUILD_AND_TEST_WITH_CI=ON -DBUILD_VERIFICATION=OFF -DVERBOSE_SYSTEM_SUPPORT_WARNING=ON \
                 ${CMAKE_OPTIONS}
    or_die make -j

    # Test 1
    or_die ctest -T test --output-on-failure -V
    rm -rf ${METALL_TEST_DIR}

    # Test 2
    cd bench/adjacency_list
    or_die bash ../../../bench/adjacency_list/test/test.sh -d${METALL_TEST_DIR}
    rm -rf ${METALL_TEST_DIR}

    # Test 3
    or_die bash ../../../bench/adjacency_list/test/test_large.sh -d${METALL_TEST_DIR}
    rm -rf ${METALL_TEST_DIR}

    cd ${METALL_ROOT_DIR}
    rm -rf ./build
}

main() {

  setup_test_dir

# USE_SPACE_AWARE_BIN
# DISABLE_FREE_FILE_SPACE
# DISABLE_SMALL_OBJECT_CACHE
# FREE_SMALL_OBJECT_SIZE_HINT

  for BUILD_TYPE in Debug Release RelWithDebInfo; do
    for USE_SPACE_AWARE_BIN in ON OFF; do
      for DISABLE_FREE_FILE_SPACE in ON OFF; do
        for DISABLE_SMALL_OBJECT_CACHE in ON OFF; do
          for FREE_SMALL_OBJECT_SIZE_HINT in 0 8 4096 8192; do
            run_buid_and_test_core -DCMAKE_BUILD_TYPE=${BUILD_TYPE}
                                   -DUSE_SPACE_AWARE_BIN=${USE_SPACE_AWARE_BIN} \
                                   -DDISABLE_FREE_FILE_SPACE=${DISABLE_FREE_FILE_SPACE} \
                                   -DDISABLE_SMALL_OBJECT_CACHE=${DISABLE_SMALL_OBJECT_CACHE} \
                                   -DFREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT}
          done
        done
      done
    done
  done
}

main "$@"