// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_MUTEX_HPP
#define METALL_MUTEX_HPP

#include <mutex>
#include <cassert>

namespace metall::utility {
namespace mutex {

/// \brief A utility function that returns a mutex lock allocated as a static object
/// This is an experimental implementation
/// \example
/// { // Mutex region
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

}
}

#endif //METALL_MUTEX_HPP
