// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_STRING_HPP
#define METALL_JSON_STRING_HPP

#include <cstring>

#include <metall/json/json_fwd.hpp>

namespace metall::json::jsndtl {

template <typename char_t, typename traits, typename allocator,
          typename other_string_type>
inline bool general_string_equal(
    const basic_string<char_t, traits, allocator> &string,
    const other_string_type &other_string) noexcept {
  return std::strcmp(string.c_str(), other_string.c_str()) == 0;
}

}  // namespace metall::json::jsndtl

#endif  // METALL_JSON_STRING_HPP
