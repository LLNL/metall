// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_UTILITY_MEMORY_HPP
#define METALL_BENCH_UTILITY_MEMORY_HPP

#include <metall/detail/utility/memory.hpp>

namespace utility {
  using metall::detail::utility::get_page_size;
  using metall::detail::utility::get_used_ram_size;
  using metall::detail::utility::get_num_page_faults;
} // namespace utility

#endif //METALL_UTILITY_MEMORY_HPP
