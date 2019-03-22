// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_METALL_HPP
#define METALL_METALL_HPP

#include <metall/v0/manager_v0.hpp>

namespace metall {

namespace detail {
using v0_chunk_no_type = uint32_t;
constexpr std::size_t k_v0_chunk_size = 1 << 21;
}

/// \brief Default manager type
using manager = v0::manager_v0<detail::v0_chunk_no_type, detail::k_v0_chunk_size>;

} // namespace metall

#endif //METALL_METALL_HPP
