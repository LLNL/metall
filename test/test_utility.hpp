// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TEST_UTILITY_HPP
#define METALL_TEST_UTILITY_HPP

#include "gtest/gtest.h"

#include <string>
#include <cstdlib>

#include <metall/detail/utility/file.hpp>

namespace test_utility {

const char *k_test_dir_env_name = "METALL_TEST_DIR";
const char *k_default_test_dir = "/tmp/metall_test_dir";

namespace detail {
inline std::string get_test_dir() {
  if (const char *env_p = std::getenv(k_test_dir_env_name)) {
    return std::string(env_p);
  }
  return std::string(k_default_test_dir);
}
}

inline bool create_test_dir() {
  if (!metall::detail::utility::directory_exist(detail::get_test_dir()))
    return metall::detail::utility::create_directory(detail::get_test_dir());
  return true;
}

inline std::string make_test_path(const std::string &name = std::string()) {
  return detail::get_test_dir() + "/metalltest"
    + "-" + ::testing::UnitTest::GetInstance()->current_test_case()->name()
    + "-" + ::testing::UnitTest::GetInstance()->current_test_info()->name()
    + "-" + name;
}

}
#endif //METALL_TEST_UTILITY_HPP
