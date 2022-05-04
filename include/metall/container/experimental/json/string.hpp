// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_HPP

#include <cstring>

namespace metall::container::experimental::json {
namespace jsndtl {

template <typename char_t, typename traits, typename allocator, typename other_string_type>
inline bool general_string_equal(const string<char_t, traits, allocator> &string,
                                 const other_string_type &other_string) noexcept {
  return std::strcmp(string.c_str(), other_string.c_str()) == 0;
}

} // namespace jsndtl
} // namespace metall::container::experimental::json {

#endif //METALL_CONTAINER_EXPERIMENT_STRING_HPP
