// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_HASH_HPP
#define METALL_UTILITY_HASH_HPP

#include <metall/detail/utility/hash.hpp>

namespace metall::utility {

/// \brief Hash a value of type T
/// \tparam T The type of a value to hash
/// \tparam seed A seed value used for hashing
template <typename T, unsigned int seed = 123>
using hash = metall::detail::utility::hash<T, seed>;

/// \brief Hash string data
/// \tparam string_type A string class
/// \tparam seed A seed value used for hashing
template <typename string_type, unsigned int seed = 123>
using string_hash = metall::detail::utility::string_hash<string_type, seed>;

} // namespace metall::utility

#endif //METALL_UTILITY_HASH_HPP
