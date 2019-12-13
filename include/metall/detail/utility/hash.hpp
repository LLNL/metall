// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_HASH_HPP
#define METALL_DETAIL_UTILITY_HASH_HPP

#include <cstdint>

namespace metall {
namespace detail {
namespace utility {

// -----------------------------------------------------------------------------
// This also contains public domain code from MurmurHash2.
// From the MurmurHash2 header:
//
// MurmurHash2 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//
//-----------------------------------------------------------------------------
inline constexpr uint64_t MurmurHash64A(const void *key, const int len, const uint64_t seed) {
  constexpr uint64_t m = 0xc6a4a7935bd1e995ULL;
  constexpr int r = 47;

  uint64_t h = seed ^(len * m);

  const uint64_t *data = (const uint64_t *)key;
  const uint64_t *end = data + (len / 8);

  while (data != end) {
    uint64_t k = *data++;

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  const unsigned char *data2 = (const unsigned char *)data;

  switch (len & 7) {
    case 7: h ^= uint64_t(data2[6]) << 48; [[fallthrough]];
    case 6: h ^= uint64_t(data2[5]) << 40; [[fallthrough]];
    case 5: h ^= uint64_t(data2[4]) << 32; [[fallthrough]];
    case 4: h ^= uint64_t(data2[3]) << 24; [[fallthrough]];
    case 3: h ^= uint64_t(data2[2]) << 16; [[fallthrough]];
    case 2: h ^= uint64_t(data2[1]) << 8; [[fallthrough]];
    case 1: h ^= uint64_t(data2[0]);
      h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
};

template <typename T, unsigned int seed = 123>
struct hash {
  T operator()(const T& key) const {
    return static_cast<T>(MurmurHash64A(&key, sizeof(T), seed));
  }
};

} // namespace utility
} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_UTILITY_HASH_HPP
