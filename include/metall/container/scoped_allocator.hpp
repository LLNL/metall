// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_SCOPED_ALLOCATOR_HPP
#define METALL_CONTAINER_SCOPED_ALLOCATOR_HPP

#include <boost/container/scoped_allocator.hpp>

namespace metall::container {

/// \brief An allocator which can be used with multilevel containers.
template <class OuterAlloc, class... InnerAlloc>
using scoped_allocator_adaptor =
    boost::container::scoped_allocator_adaptor<OuterAlloc, InnerAlloc...>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_SCOPED_ALLOCATOR_HPP
