// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
#define METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP

#include <iostream>
#include <fstream>
#include <metall/detail/utility/memory.hpp>
#include <metall/detail/utility/logger.hpp>

namespace metall {
namespace detail {
namespace utility {

inline bool reset_soft_dirty_bit() {
  std::ofstream ofs("/proc/self/clear_refs");
  if (!ofs.is_open()) {
    log::out(log::level::error, __FILE__, __LINE__, "Cannot open file clear_refs");
    return false;
  }

  ofs << "4";
  ofs.close();

  if (!ofs) {
    log::out(log::level::error, __FILE__, __LINE__, "Cannot write to /proc/self/clear_refs");
    return false;
  }

  return true;
}

inline constexpr bool check_soft_dirty_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 55ULL) & 1ULL;
}

inline constexpr bool check_swapped_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 62ULL) & 1ULL;
}

inline constexpr bool check_present_page(const uint64_t pagemap_value) {
  return (pagemap_value >> 63ULL) & 1ULL;
}

} // namespace utility
} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_UTILITY_SOFT_DIRTY_PAGE_HPP
