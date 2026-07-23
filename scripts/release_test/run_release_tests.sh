#!/bin/bash

# Runs all test cases with all GCC and boost combinations.
# Usage:
#   Run this script from the root directory of Metall
#   bash ./scripts/release_test/run_release_tests.sh

source ./scripts/test_utility.sh

CC_COMPILERS=('gcc')
CPP_COMPILERS=('g++')
MPI_CC_COMPILERS=('mpicc')
MPI_CPP_COMPILERS=('mpicxx')

BOOST_URLS=('https://github.com/boostorg/boost/releases/download/boost-1.91.0-1/boost-1.91.0-1-cmake.tar.gz'
'https://github.com/boostorg/boost/releases/download/boost-1.89.0/boost-1.89.0-cmake.tar.gz'
'https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz'
'https://github.com/boostorg/boost/releases/download/boost-1.87.0/boost-1.87.0-cmake.tar.gz'
'https://github.com/boostorg/boost/releases/download/boost-1.86.0/boost-1.86.0-cmake.tar.xz'
'https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz'
'https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.gz'
''
)

for CC_COMPILER in "${CC_COMPILERS[@]}"; do
  for CPP_COMPILER in "${CPP_COMPILERS[@]}"; do
    for BOOST_URL in "${BOOST_URLS[@]}"; do
      BOOST_VER=$(basename "${BOOST_URL}" | sed -E 's/boost-([0-9.]+(-[0-9]+)?)-cmake.tar.(gz|xz)/\1/')
      export METALL_TEST_DIR="/dev/shm/metall_test_${CC_COMPILER}_${CPP_COMPILER}_boost${BOOST_VER}"
      export METALL_BUILD_DIR="/dev/shm/metall_test_build_${CC_COMPILER}_${CPP_COMPILER}_boost${BOOST_VER}"

    export METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_INCLUDE_ROOT=${BOOST_ROOT}/include"

    or_die bash ./scripts/release_test/run_intensive_test.sh
  done
done

echo "Passed All Tests!!"
