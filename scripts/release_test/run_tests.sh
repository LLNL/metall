#!/bin/sh

# Runs all test cases with all GCC and boost combinations
# Usage:
#   Run this script from the root directory of Metall
#   sh ./scripts/release_test/run_tests.sh

source ./scripts/test_utility.sh

GCC_VERSIONS=('10.2.0' '10.1.0' '9.3.0' '9.2.0' '9.1.0' '8.3.0' '8.2.0' '8.1.0')
BOOST_VERSiONS=('1.74.0' '1.73.0' '1.72.0' '1.71.0' '1.70.0' '1.69.0' '1.68.0' '1.67.0' '1.66.0' '1.65.1' '1.65.0' '1.64.0')

for GCC_VER in "${GCC_VERSIONS[@]}"; do
  for BOOST_VER in "${BOOST_VERSiONS[@]}"; do
    export METALL_TEST_DIR="/dev/shm/metall_test_gcc${GCC_VER}_boost${BOOST_VER}"
    export METALL_BUILD_DIR="/dev/shm/metall_test_build_gcc${GCC_VER}_boost${BOOST_VER}"

    spack load gcc@${GCC_VER}
    spack load boost@${BOOST_VER}

    or_die sh ./scripts/release_test/full_build_and_test.sh
  done
done

echo "Passed All Tests!!"
