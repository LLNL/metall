// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_HASH_HPP
#define METALL_DETAIL_UTILITY_HASH_HPP

#include <cstdint>

#include <metall/detail/utilities.hpp>

namespace metall::mtlldetail {

// -----------------------------------------------------------------------------
// This file contains public domain code from MurmurHash2.
// From the MurmurHash2 header:
//
// MurmurHash2 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//-----------------------------------------------------------------------------

namespace mmhdtl {
//-----------------------------------------------------------------------------
// Block read - on little-endian machines this is a single load,
// while on big-endian or unknown machines the byte accesses should
// still get optimized into the most efficient instruction.
static inline uint64_t murmurhash_getblock(const uint64_t *p) {
#if defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
  return *p;
#else
  const uint8_t *c = (const uint8_t *)p;
  return (uint64_t)c[0] | (uint64_t)c[1] << 8 | (uint64_t)c[2] << 16 |
         (uint64_t)c[3] << 24 | (uint64_t)c[4] << 32 | (uint64_t)c[5] << 40 |
         (uint64_t)c[6] << 48 | (uint64_t)c[7] << 56;
#endif
}

}  // namespace mmhdtl

/// \brief MurmurHash2 64-bit hash for 64-bit platforms.
/// \param key The key to hash.
/// \param len Length of the key in byte.
/// \param seed A seed value used for hashing.
/// \return A hash value.
METALL_PRAGMA_IGNORE_GCC_UNINIT_WARNING_BEGIN
inline uint64_t murmur_hash_64a(const void *key, int len,
                                uint64_t seed) noexcept {
  const uint64_t m = 0xc6a4a7935bd1e995LLU;
  const int r = 47;

  uint64_t h = seed ^ (len * m);

  const uint64_t *data = (const uint64_t *)key;
  const uint64_t *end = data + (len / 8);

  while (data != end) {
    uint64_t k = mmhdtl::murmurhash_getblock(data++);

    k *= m;
    k ^= k >> r;
    k *= m;

    h ^= k;
    h *= m;
  }

  const unsigned char *data2 = (const unsigned char *)data;

  switch (len & 7) {
    case 7:
      h ^= uint64_t(data2[6]) << 48;
      [[fallthrough]];
    case 6:
      h ^= uint64_t(data2[5]) << 40;
      [[fallthrough]];
    case 5:
      h ^= uint64_t(data2[4]) << 32;
      [[fallthrough]];
    case 4:
      h ^= uint64_t(data2[3]) << 24;
      [[fallthrough]];
    case 3:
      h ^= uint64_t(data2[2]) << 16;
      [[fallthrough]];
    case 2:
      h ^= uint64_t(data2[1]) << 8;
      [[fallthrough]];
    case 1:
      h ^= uint64_t(data2[0]);
      h *= m;
  };

  h ^= h >> r;
  h *= m;
  h ^= h >> r;

  return h;
}
METALL_PRAGMA_IGNORE_GCC_UNINIT_WARNING_END

/// \brief Alias of murmur_hash_64a.
/// \warning This function is deprecated. Use murmur_hash_64a instead.
inline uint64_t MurmurHash64A(const void *key, int len,
                              uint64_t seed) noexcept {
  return murmur_hash_64a(key, len, seed);
}

/// \brief Hash a value of type T. Provides the same interface as std::hash.
/// \tparam seed A seed value used for hashing.
template <unsigned int seed = 123>
struct hash {
  template <typename T>
  inline std::size_t operator()(const T &key) const noexcept {
    return murmur_hash_64a(&key, sizeof(T), seed);
  }
};

/// \brief Hash string data.
/// \tparam seed A seed value used for hashing.
template <unsigned int seed = 123>
struct str_hash {
  using is_transparent = void;

  template <typename string_type>
  inline std::size_t operator()(const string_type &str) const noexcept {
    if constexpr (std::is_same_v<string_type, char *> ||
                  std::is_same_v<string_type, const char *>) {
      return murmur_hash_64a(str, std::char_traits<char>::length(str), seed);
    } else {
      return murmur_hash_64a(
          str.c_str(), str.length() * sizeof(typename string_type::value_type),
          seed);
    }
  }
};

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_HASH_HPP
