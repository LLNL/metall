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
run_build_and_test_kernel() {
  source ${METALL_ROOT_DIR}/scripts/test_utility.sh

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
# Builds documents
# Globals:
#   METALL_ROOT_DIR
#   METALL_BUILD_DIR
# Outputs: STDOUT and STDERR
#######################################
build_docs() {
  source ${METALL_ROOT_DIR}/scripts/test_utility.sh

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