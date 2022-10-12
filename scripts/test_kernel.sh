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

  local test_dir=${METALL_TEST_DIR}
  if [ -d ${test_dir} ]; then
    test_dir="${test_dir}/metall-test-${RANDOM}"
  fi

  local build_dir=${METALL_BUILD_DIR}
  if [ -d ${build_dir} ]; then
    build_dir="${build_dir}/metall-build-${RANDOM}"
  fi

  mkdir -p ${build_dir}
  pushd ${build_dir}
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
  rm -rf ${test_dir}
  or_die ctest --timeout 1000

  # Test 2
  rm -rf ${test_dir}
  pushd bench/adjacency_list
  or_die bash ./test/test.sh -d${test_dir}
  popd

  # Test 3
  rm -rf ${test_dir}
  pushd bench/adjacency_list
  or_die bash ./test/test_large.sh -d${test_dir}
  popd

  # TODO: reflink test and C_API test

  rm -rf ${test_dir}
  popd
  rm -rf ${build_dir}
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

  local build_dir=${METALL_BUILD_DIR}
  if [ -d ${build_dir} ]; then
    build_dir="${build_dir}/metall-build-${RANDOM}"
  fi

  mkdir -p ${build_dir}
  cd ${build_dir}
  echo "Build and test in ${PWD}"

  # Build
  local CMAKE_FILE_LOCATION=${METALL_ROOT_DIR}
  or_die cmake ${CMAKE_FILE_LOCATION} -DBUILD_DOC=ON
  or_die make build_doc

  cd ../
  rm -rf ${build_dir}
}