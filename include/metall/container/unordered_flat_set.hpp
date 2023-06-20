// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_UNORDERED_FLAT_SET_HPP
#define METALL_CONTAINER_UNORDERED_FLAT_SET_HPP

#include <functional>

static_assert(BOOST_VERSION >= 108100, "Unsupported Boost version");
#include <boost/unordered/unordered_flat_set.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief An unordered_flat_set container that uses Metall as its default
/// allocator.
template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = manager::allocator_type<Key>>
using unordered_flat_set =
    boost::unordered_flat_set<Key, Hash, KeyEqual, Allocator>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_UNORDERED_FLAT_SET_HPP
