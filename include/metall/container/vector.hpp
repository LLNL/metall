// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_VECTOR_HPP
#define METALL_CONTAINER_VECTOR_HPP

#include <boost/container/vector.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief A vector container that uses Metall as its default allocator.
template <typename T, typename Allocator = manager::allocator_type<T>>
using vector = boost::container::vector<T, Allocator>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_VECTOR_HPP
