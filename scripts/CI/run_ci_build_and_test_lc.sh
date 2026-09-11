#!/bin/bash
set -e

module load ${COMPILER}/${COMPILER_VERSION}

export METALL_LIMIT_MAKE_PARALLELS=${METALL_LIMIT_MAKE_PARALLELS:-8}
export METALL_TEST_DIR=${METALL_TEST_DIR:-${CI_JOB_NAME:-test_job}}

BOOST_VERSION=${BOOST_VERSION:-"1.92.0"}
TEST_TYPE=${TEST_TYPE:-"build_and_test"}

echo "Build and test on ${HOSTNAME}"
echo "BOOST_VERSION=${BOOST_VERSION}"
echo "TEST_TYPE=${TEST_TYPE}"
echo "gcc version: $(gcc --version | head -n 1)"

WORK_DIR=$(mktemp -d -p /dev/shm boost_ci_XXXXXX 2>/dev/null || mktemp -d -t boost_ci_XXXXXX)
trap 'rm -rf "${WORK_DIR}"' EXIT

pushd "${WORK_DIR}" > /dev/null

if [[ -n "${BOOST_UNCOMPRESS}" && "${BOOST_UNCOMPRESS}" == "true" ]]; then
  mkdir boost
  tar xf "${BOOST_PATH}" -C boost --strip-components 1
  export METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_INCLUDE_ROOT=${PWD}/boost"
else
  export METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_FETCH_URL=${BOOST_PATH}"
fi

popd > /dev/null


if [[ "${TEST_TYPE}" == "consumer_smoke" ]]; then
  bash ./scripts/CI/run_ci_install_tree_consumer_smoke_test.sh
else
  bash ./scripts/CI/run_ci_build_and_test.sh
fi
