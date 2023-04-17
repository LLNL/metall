// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_MUTEX_HPP
#define METALL_UTILITY_MUTEX_HPP

#include <mutex>
#include <cassert>

namespace metall::utility {

/// \namespace metall::utility::mutex
/// \brief Namespace for mutex
namespace mutex {

/// \brief A utility function that returns a mutex lock allocated as a static
/// object. This is an experimental implementation. Example: { // Mutex region
///   const int bank_index = hash(key) % num_banks;
///   auto guard = metall::utility::mutex::mutex_lock<num_banks>(bank_index);
///   // do some mutex work
/// }
template <int num_banks>
inline std::unique_lock<std::mutex> mutex_lock(const std::size_t index) {
  static std::mutex mutexes[num_banks];
  assert(index < num_banks);
  return std::unique_lock<std::mutex>(mutexes[index]);
}

}  // namespace mutex
}  // namespace metall::utility

/// \example static_mutex.cpp
/// This is an example of how to use the mutex_lock function.

#endif  // METALL_UTILITY_MUTEX_HPP
