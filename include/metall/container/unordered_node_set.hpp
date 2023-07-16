// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_UNORDERED_NODE_SET_HPP
#define METALL_CONTAINER_UNORDERED_NODE_SET_HPP

#include <functional>

static_assert(BOOST_VERSION >= 108200, "Unsupported Boost version");
#include <boost/unordered/unordered_node_set.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief An unordered_node_set container that uses Metall as its default
/// allocator.
template <class Key, class Hash = std::hash<Key>,
          class KeyEqual = std::equal_to<Key>,
          class Allocator = manager::allocator_type<Key>>
using unordered_node_set =
    boost::unordered_node_set<Key, Hash, KeyEqual, Allocator>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_UNORDERED_NODE_SET_HPP
