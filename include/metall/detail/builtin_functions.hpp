// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_BUILTIN_FUNCTIONS_HPP
#define METALL_DETAIL_UTILITY_BUILTIN_FUNCTIONS_HPP

namespace metall::mtlldetail {

/// \brief Count Leading Zeros.
inline int clzll(const unsigned long long x) noexcept {
#if defined(__GNUG__) || defined(__clang__)
  return __builtin_clzll(x);
#else
#error "GCC or Clang must be used to use __builtin_clzll" << std::endl;
#endif
}

/// \brief Count Trailing Zeros.
inline int ctzll(const unsigned long long x) noexcept {
#if defined(__GNUG__) || defined(__clang__)
  return __builtin_ctzll(x);
#else
#error "GCC or Clang must be used to use __builtin_ctzll" << std::endl;
#endif
}

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_BUILTIN_FUNCTIONS_HPP
