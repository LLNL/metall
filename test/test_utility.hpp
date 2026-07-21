// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TEST_UTILITY_HPP
#define METALL_TEST_UTILITY_HPP

#include "gtest/gtest.h"

#include <string>
#include <cerrno>
#include <cstdlib>
#include_next <sstream>
#include <filesystem>

#include <metall/detail/file.hpp>

namespace test_utility {

namespace {
namespace fs = std::filesystem;
}

const char *k_test_dir_env_name = "METALL_TEST_DIR";
const char *k_default_test_dir = "/tmp/metall_test_dir";

namespace detail {
inline fs::path get_test_dir() {
  if (const char *env_p = std::getenv(k_test_dir_env_name)) {
    return fs::path(env_p);
  }
  return fs::path(k_default_test_dir);
}
}  // namespace detail

inline bool create_test_dir() {
  if (!metall::mtlldetail::directory_exist(detail::get_test_dir()))
    return metall::mtlldetail::create_directory(detail::get_test_dir());
  return true;
}

namespace detail {
// Test executables can contain identically named test cases (for example,
// manager_test and manager_test_single_thread). The program name keeps their
// datastore paths distinct when ctest runs them concurrently.
inline const char *get_program_name() {
#if defined(__GLIBC__)
  return ::program_invocation_short_name;
#elif defined(__APPLE__) || defined(__FreeBSD__)
  return ::getprogname();
#else
  return "unknown";
#endif
}
}  // namespace detail

inline fs::path make_test_path(const fs::path &name = fs::path()) {
  std::stringstream file_name;
  file_name << "metalltest-" << detail::get_program_name() << "-"
            << ::testing::UnitTest::GetInstance()->current_test_case()->name()
            << "-"
            << ::testing::UnitTest::GetInstance()->current_test_info()->name()
            << "-" << name.string();
  return detail::get_test_dir() / file_name.str();
}

}  // namespace test_utility
#endif  // METALL_TEST_UTILITY_HPP
