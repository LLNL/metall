// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_UNORDERED_MAP_HPP
#define METALL_CONTAINER_UNORDERED_MAP_HPP

#include <functional>

#include <boost/unordered_map.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief An unordered_map container that uses Metall as its default allocator.
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = manager::allocator_type<std::pair<const Key, T>>>
using unordered_map = boost::unordered_map<Key, T, Hash, KeyEqual, Allocator>;

/// \brief An unordered_multimap container that uses Metall as its default
/// allocator.
template <class Key, class T, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = manager::allocator_type<std::pair<const Key, T>>>
using unordered_multimap =
    boost::unordered_multimap<Key, T, Hash, KeyEqual, Allocator>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_UNORDERED_MAP_HPP
