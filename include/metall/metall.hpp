// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_METALL_HPP
#define METALL_METALL_HPP

#include <metall/basic_manager.hpp>

namespace metall {

/// \brief Basic Metall manager type
//template <typename chunk_no_type, std::size_t chunk_size, typename kernel_allocator_type>
//using basic_manager = basic_manager<chunk_no_type, chunk_size, kernel_allocator_type>;

/// \brief Metall manager type
using manager = basic_manager<>;

} // namespace metall

#endif //METALL_METALL_HPP
