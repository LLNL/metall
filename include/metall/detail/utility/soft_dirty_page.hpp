// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
#define METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP

#include <iostream>
#include <fstream>
#include <metall/detail/utility/memory.hpp>

namespace metall {
namespace detail {
namespace utility {

bool reset_soft_dirty() {
  std::ofstream ofs("/proc/self/clear_refs");
  if (!ofs.is_open()) {
    std::cerr << "Cannot open file clear_refs" << std::endl;
    return false;
  }

  ofs << "4" << std::endl;
  ofs.close();

  return true;
}

constexpr bool check_soft_dirty(const uint64_t pagemap_value) {
  return pagemap_value & 0x80000000000000ULL;
}

} // namespace utility
} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
