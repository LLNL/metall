// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TEST_UTILITY_HPP
#define METALL_TEST_UTILITY_HPP

#include "gtest/gtest.h"

#include <string>
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

inline fs::path make_test_path(const fs::path &name = fs::path()) {
  std::stringstream file_name;
  file_name << "metalltest-"
            << ::testing::UnitTest::GetInstance()->current_test_case()->name()
            << "-"
            << ::testing::UnitTest::GetInstance()->current_test_info()->name()
            << "-" << name.string();
  return detail::get_test_dir() / file_name.str();
}

}  // namespace test_utility
#endif  // METALL_TEST_UTILITY_HPP
