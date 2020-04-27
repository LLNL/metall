#!/bin/sh

# Bash script that builds and tests Metall with many compile time configurations
# 1. Set environmental variables for build
# Set manually:
# export CC=gcc
# export CXX=g++
# export CMAKE_PREFIX_PATH=${CMAKE_PREFIX_PATH}:/path/to/boost
#
# Or, config environmental variables using spack:
# spack load g++
# spack load boost
#
# 2. Set environmental variables for test
# (option)
# export METALL_TEST_DIR=/tmp
# export METALL_LIMIT_MAKE_PARALLELS=n
#
# 3. Run the script
# sh ./scripts/CI/travis_ci/build_and_test.sh

METALL_ROOT_DIR=${PWD}

or_die () {
  echo "Command: " "$@"
  echo ">>>>>>>>>>"
  "$@"
  local status=$?
  if [[ $status != 0 ]] ; then
      echo ERROR $status command: $@
      exit $status
  fi
  echo "<<<<<<<<<<"
}

exec () {
  echo "Command: " "$@"
  echo ">>>>>>>>>>"
  "$@"
  echo "<<<<<<<<<<"
}

# As multiple CI jobs could run on the same machine
# generate an unique test dir for each CI job
# MEMO: setup a filesystem in local
# truncate --size 512m XFSfile
# mkfs.xfs -m crc=1 -m reflink=1 XFSfile
# mkdir XFSmountpoint
# mount -o loop XFSfile XFSmountpoint
# xfs_info XFSmountpoint
# cd XFSmountpoint
setup_test_dir() {
  if [[ -z "${METALL_TEST_DIR}" ]]; then
    METALL_TEST_DIR="/tmp/${RANDOM}"
  fi

  # mkdir -p ${METALL_TEST_DIR} # Metall creates automatically if the directory does not exist
  echo "Store test data to ${METALL_TEST_DIR}"
}

run_buid_and_test_core() {
  mkdir -p ./build
  cd build
  echo "Build and test in ${PWD}"

  # Build
  local CMAKE_OPTIONS="$@"
  local CMAKE_FILE_LOCATION=${METALL_ROOT_DIR}
  or_die cmake ${CMAKE_FILE_LOCATION} \
               -DBUILD_BENCH=ON -DBUILD_TEST=ON -DRUN_LARGE_SCALE_TEST=ON -DBUILD_DOC=OFF  -DBUILD_C=ON \
               -DRUN_BUILD_AND_TEST_WITH_CI=ON -DBUILD_VERIFICATION=OFF -DVERBOSE_SYSTEM_SUPPORT_WARNING=OFF \
               ${CMAKE_OPTIONS}
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
  cd bench/adjacency_list
  or_die bash ${METALL_ROOT_DIR}/bench/adjacency_list/test/test.sh -d${METALL_TEST_DIR}
  cd ../../

  # Test 3
  rm -rf ${METALL_TEST_DIR}
  cd bench/adjacency_list
  or_die bash ${METALL_ROOT_DIR}/bench/adjacency_list/test/test_large.sh -d${METALL_TEST_DIR}
  cd ../../

  # TODO: reflink test and C_API test

  rm -rf ${METALL_TEST_DIR}
  cd ../
  rm -rf ./build
}

show_system_info() {
  exec df -h
  exec df -ih
  exec free -g
  exec uname -r
}

main() {
  show_system_info

  echo "Build and test on ${HOSTNAME}"

  setup_test_dir
  export METALL_TEST_DIR

  for BUILD_TYPE in Debug; do
    for DISABLE_FREE_FILE_SPACE in ON OFF; do
      for DISABLE_SMALL_OBJECT_CACHE in OFF; do
        for FREE_SMALL_OBJECT_SIZE_HINT in 0 8192; do
          run_buid_and_test_core -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                                 -DDISABLE_FREE_FILE_SPACE=${DISABLE_FREE_FILE_SPACE} \
                                 -DDISABLE_SMALL_OBJECT_CACHE=${DISABLE_SMALL_OBJECT_CACHE} \
                                 -DFREE_SMALL_OBJECT_SIZE_HINT=${FREE_SMALL_OBJECT_SIZE_HINT}
        done
      done
    done
  done
}

main "$@"