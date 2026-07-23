#!/bin/bash

# Runs all test cases with all GCC and boost combinations utilizing Spack.
# Usage:
#   Run this script from the root directory of Metall
#   bash ./scripts/release_test/run_release_tests_spack.sh

source ./scripts/test_utility.sh

GCC_VERSIONS=('14.2.0' '14.1.0' '13.3.0' '13.2.0' '13.1.0' '12.4.0' '12.3.0' '12.2.0' '12.1.0' '11.5.0' '11.4.0' '11.3.0' '11.2.0' '11.1.0' '10.5.0' '10.4.0' '10.3.0' '10.2.0' '10.1.0' '9.5.0' '9.4.0' '9.3.0' '9.2.0' '9.1.0' '8.5.0' '8.4.0' '8.3.0' '8.2.0' '8.1.0')
BOOST_VERSIONS=('1.86.0' '1.85.0' '1.84.0' '1.83.0' '1.82.0' '1.81.0' '1.80.0')

# Check if Spack is installed
if ! command -v spack &> /dev/null; then
  echo "Spack is not installed. Please install Spack and try again."
  exit 1
fi

for GCC_VER in "${GCC_VERSIONS[@]}"; do
  for BOOST_VER in "${BOOST_VERSIONS[@]}"; do
    export METALL_TEST_DIR="/dev/shm/metall_test_gcc${GCC_VER}_boost${BOOST_VER}"
    export METALL_BUILD_DIR="/dev/shm/metall_test_build_gcc${GCC_VER}_boost${BOOST_VER}"

    spack load gcc@${GCC_VER}

    # Assumes that Spack set BOOST_ROOT
    spack load boost@${BOOST_VER}
    export METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_INCLUDE_ROOT=${BOOST_ROOT}/include"

    or_die bash ./scripts/release_test/run_intensive_test.sh
  done
done

echo "Passed All Tests!!"
