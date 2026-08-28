#!/bin/bash

##############################################################################
# Bash script that verifies Metall can be consumed by external CMake projects
# via both find_package(Metall) and FetchContent.
#
# 1. Set environmental variables for CMake configuration and build, if needed.
# 2. Run this script from the root directory of Metall.
# cd metall # Metall root directory
# bash ./scripts/CI/run_ci_install_tree_consumer_smoke_test.sh
##############################################################################

#######################################
# main function
# This function is required to be called from the root directory of Metall.
#
# Used environmental variables:
#   METALL_BUILD_DIR (option)
#   METALL_CMAKE_ADDITIONAL_OPTIONS (option)
#     E.g. METALL_CMAKE_ADDITIONAL_OPTIONS="-DBOOST_FETCH_URL=/path/to/boost.tar.gz"
#   METALL_LIMIT_MAKE_PARALLELS (option)
# Outputs: STDOUT and STDERR
#######################################

# Keep the producer and consumer build invocations aligned while still
# respecting CI jobs that cap parallelism explicitly.
build_with_optional_parallel() {
  local build_dir="$1"

  if [[ -z "${METALL_LIMIT_MAKE_PARALLELS}" ]]; then
    or_die cmake --build "${build_dir}" --parallel
  else
    or_die cmake --build "${build_dir}" --parallel "${METALL_LIMIT_MAKE_PARALLELS}"
  fi
}

# Both examples create the same datastore path, so remove it between runs to
# keep the smoke test deterministic.
run_consumer_examples() {
  local build_dir="$1"

  rm -rf /tmp/dir
  or_die "${build_dir}/cpp_example"

  if [[ -x "${build_dir}/c_example" ]]; then
    rm -rf /tmp/dir
    or_die "${build_dir}/c_example"
  fi
}

# Verify install-tree consumption through MetallConfig.cmake.
run_find_package_consumer_smoke_test() {
  local source_dir="$1"
  local build_dir="$2"
  local install_prefix="$3"

  or_die cmake -S "${source_dir}" -B "${build_dir}" \
    -DCMAKE_PREFIX_PATH="${install_prefix}"
  build_with_optional_parallel "${build_dir}"
  run_consumer_examples "${build_dir}"
}

# Verify source-tree consumption through FetchContent while forcing Metall to
# resolve from the current checkout instead of cloning another copy.
run_fetchcontent_consumer_smoke_test() {
  local source_dir="$1"
  local build_dir="$2"
  local metall_root_dir="$3"
  shift 3
  local additional_cmake_options=("$@")

  or_die cmake -S "${source_dir}" -B "${build_dir}" \
    -DBUILD_C=ON \
    -DFETCHCONTENT_SOURCE_DIR_METALL="${metall_root_dir}" \
    "${additional_cmake_options[@]}"
  build_with_optional_parallel "${build_dir}"
  run_consumer_examples "${build_dir}"
}

main() {
  readonly METALL_ROOT_DIR=${PWD}
  source "${METALL_ROOT_DIR}/scripts/test_utility.sh"

  echo "Install-tree smoke test on ${HOSTNAME}"
  show_system_info

  if [[ -z "${METALL_BUILD_DIR}" ]]; then
    readonly METALL_BUILD_DIR="${METALL_ROOT_DIR}/build_${RANDOM}"
  fi

  local additional_cmake_options=()
  if [[ -n "${METALL_CMAKE_ADDITIONAL_OPTIONS}" ]]; then
    read -r -a additional_cmake_options <<< "${METALL_CMAKE_ADDITIONAL_OPTIONS}"
  fi

  local smoke_root="${METALL_BUILD_DIR}/install-tree-smoke-${RANDOM}"
  local producer_build_dir="${smoke_root}/producer-build"
  local find_package_build_dir="${smoke_root}/find-package-build"
  local fetchcontent_build_dir="${smoke_root}/fetchcontent-build"
  local install_prefix="${smoke_root}/install"
  local find_package_source_dir="${METALL_ROOT_DIR}/example/cmake/find_package"
  local fetchcontent_source_dir="${METALL_ROOT_DIR}/example/cmake/FetchContent"
  local producer_cmake_options=(
    -DCMAKE_BUILD_TYPE=Release
    -DCMAKE_INSTALL_PREFIX="${install_prefix}"
    -DBUILD_BENCH=OFF
    -DBUILD_TEST=OFF
    -DRUN_LARGE_SCALE_TEST=OFF
    -DBUILD_DOC=OFF
    -DBUILD_C=ON
    -DBUILD_UTILITY=OFF
    -DBUILD_EXAMPLE=OFF
    -DRUN_BUILD_AND_TEST_WITH_CI=ON
    -DBUILD_VERIFICATION=OFF
  )

  producer_cmake_options+=("${additional_cmake_options[@]}")

  mkdir -p "${smoke_root}"

  # Install a minimal producer tree first so the downstream find_package()
  # example exercises the exported package configuration rather than the source
  # tree directly.
  or_die cmake -S "${METALL_ROOT_DIR}" -B "${producer_build_dir}" "${producer_cmake_options[@]}"
  build_with_optional_parallel "${producer_build_dir}"

  or_die cmake --install "${producer_build_dir}"

  run_find_package_consumer_smoke_test \
    "${find_package_source_dir}" \
    "${find_package_build_dir}" \
    "${install_prefix}"

  run_fetchcontent_consumer_smoke_test \
    "${fetchcontent_source_dir}" \
    "${fetchcontent_build_dir}" \
    "${METALL_ROOT_DIR}" \
    "${additional_cmake_options[@]}"

  rm -rf /tmp/dir
  rm -rf "${smoke_root}"
}

main "$@"
