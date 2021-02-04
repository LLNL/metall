// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_LIST_HPP
#define METALL_CONTAINER_LIST_HPP

#include <boost/container/list.hpp>

#include <metall/metall.hpp>

namespace metall::container {

template <typename T, typename Allocator = manager::allocator_type<T>>
using list = boost::container::list<T, Allocator>;

}

#endif //METALL_CONTAINER_LIST_HPP