// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_FALLBACK_ALLOCATOR_ADAPTOR_HPP
#define METALL_UTILITY_FALLBACK_ALLOCATOR_ADAPTOR_HPP

#include "metall/container/fallback_allocator.hpp"

#warning \
    "This file is deprecated. Use container/fallback_allocator.hpp instead."

namespace metall::utility {
template <typename primary_alloc>
using fallback_allocator_adaptor =
    metall::container::fallback_allocator_adaptor<primary_alloc>;
}

#endif