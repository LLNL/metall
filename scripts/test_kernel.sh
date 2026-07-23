#######################################
# Builds and runs test programs.
# This script does not test multiple GCC and Boost versions.
# This script is designed to be called from another script.
#
# Arguments:
#   CMake options to pass
# Environmental variable arguments:
#   METALL_ROOT_DIR (required)
#   METALL_TEST_DIR (required)
#   METALL_BUILD_DIR (required)
#   METALL_LIMIT_MAKE_PARALLELS (option)
# Outputs: STDOUT and STDERR
#######################################
run_build_and_test_kernel() {
  source "${METALL_ROOT_DIR}/scripts/test_utility.sh"

  local test_dir="${METALL_TEST_DIR}"
  if [[ -d "${test_dir}" ]]; then
    test_dir="${test_dir}/metall-test-${RANDOM}"
  fi

  local build_dir="${METALL_BUILD_DIR}"
  if [[ -d "${build_dir}" ]]; then
    build_dir="${build_dir}/metall-build-${RANDOM}"
  fi

  mkdir -p "${build_dir}"
  pushd "${build_dir}"
  echo "Build and test in ${PWD}"

  # Build
  local cmake_options=("$@")
  local cmake_file_location="${METALL_ROOT_DIR}"
  printf 'cmake'
  printf ' %q' "${cmake_file_location}" "${cmake_options[@]}"
  printf '\n'
  or_die cmake "${cmake_file_location}" "${cmake_options[@]}"
  if [[ -z "${METALL_LIMIT_MAKE_PARALLELS}" ]]; then
    or_die make -j
  else
    or_die make -j${METALL_LIMIT_MAKE_PARALLELS}
  fi

  # Test 1
  rm -rf "${test_dir}"
  or_die ctest --timeout 1000

  # Test 2
  rm -rf "${test_dir}"
  pushd bench/adjacency_list
  or_die bash ./test/test.sh -d"${test_dir}"
  popd

  # Test 3
  rm -rf "${test_dir}"
  pushd bench/adjacency_list
  or_die bash ./test/test_large.sh -d"${test_dir}"
  popd

  # TODO: reflink test and C_API test

  rm -rf "${test_dir}"
  popd
  rm -rf "${build_dir}"
}


#######################################
# Builds documents
#
# Environmental variable arguments:
#   METALL_ROOT_DIR (required)
#   METALL_BUILD_DIR (required)
# Outputs: STDOUT and STDERR
#######################################
build_docs() {
  source "${METALL_ROOT_DIR}/scripts/test_utility.sh"

  local build_dir="${METALL_BUILD_DIR}"
  if [[ -d "${build_dir}" ]]; then
    build_dir="${build_dir}/metall-build-${RANDOM}"
  fi

  mkdir -p "${build_dir}"
  cd "${build_dir}"
  echo "Build and test in ${PWD}"

  # Build
  local cmake_file_location="${METALL_ROOT_DIR}"
  or_die cmake "${cmake_file_location}" -DBUILD_DOC=ON
  or_die make build_doc

  cd ../
  rm -rf "${build_dir}"
}