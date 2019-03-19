// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_MUTEX_HPP
#define METALL_DETAIL_UTILITY_MUTEX_HPP

#include <mutex>

namespace metall {
namespace detail {
namespace utility {

using mutex = std::mutex;
using mutex_lock_guard = std::lock_guard<mutex>;

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY_MUTEX_HPP
