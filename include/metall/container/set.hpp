// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_SET_HPP
#define METALL_CONTAINER_SET_HPP

#include <functional>

#include <boost/container/set.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief A set container that uses Metall as its default allocator.
template <class Key, class Compare = std::less<Key>,
          class Allocator = manager::allocator_type<Key>>
using set = boost::container::set<Key, Compare, Allocator>;

/// \brief A multiset container that uses Metall as its default allocator.
template <class Key, class Compare = std::less<Key>,
          class Allocator = manager::allocator_type<Key>>
using multiset = boost::container::multiset<Key, Compare, Allocator>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_SET_HPP
