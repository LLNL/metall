// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_MAP_HPP
#define METALL_CONTAINER_MAP_HPP

#include <functional>

#include <boost/container/map.hpp>

#include <metall/metall.hpp>

namespace metall::container {

template <class Key,
          class T,
          class Compare = std::less<Key>,
          class Allocator = manager::allocator_type<std::pair<const Key, T>>>
using map = boost::container::map<Key, T, Compare, Allocator>;

template <class Key,
          class T,
          class Compare = std::less<Key>,
          class Allocator = manager::allocator_type<std::pair<const Key, T>>>
using multimap = boost::container::multimap<Key, T, Compare, Allocator>;

}

#endif //METALL_CONTAINER_MAP_HPP
