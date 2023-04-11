// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_RANDOM_HPP
#define METALL_UTILITY_RANDOM_HPP

#include <cstdint>
#include <cstring>
#include <cassert>

namespace metall::utility {

#if !defined(DOXYGEN_SKIP)
namespace detail {

// -----------------------------------------------------------------------------
// This also contains public domain code from <http://prng.di.unimi.it/>.
// From  splitmix64.c:
// -----------------------------------------------------------------------------
//  Written in 2015 by Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// See <http://creativecommons.org/publicdomain/zero/1.0/>.
// -----------------------------------------------------------------------------
class SplitMix64 {
 public:
  explicit SplitMix64(const uint64_t seed) : m_x(seed) {}

  // This is a fixed-increment version of Java 8's SplittableRandom generator
  // See http://dx.doi.org/10.1145/2714064.2660195 and
  // http://docs.oracle.com/javase/8/docs/api/java/util/SplittableRandom.html
  //
  // It is a very fast generator passing BigCrush, and it can be useful if
  // for some reason you absolutely want 64 bits of state.
  uint64_t next() {
    uint64_t z = (m_x += 0x9e3779b97f4a7c15);
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9;
    z = (z ^ (z >> 27)) * 0x94d049bb133111eb;
    return z ^ (z >> 31);
  }

 private:
  uint64_t m_x; /* The state can be seeded with any value. */
};

// -----------------------------------------------------------------------------
// This also contains public domain code from <http://prng.di.unimi.it/>.
// From xoshiro512plusplus.c:
// -----------------------------------------------------------------------------
// Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// See <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// This is xoshiro512++ 1.0, one of our all-purpose, rock-solid
// generators. It has excellent (about 1ns) speed, a state (512 bits) that
// is large enough for any parallel application, and it passes all tests
// we are aware of.
//
// For generating just floating-point numbers, xoshiro512+ is even faster.
//
// The state must be seeded so that it is not everywhere zero. If you have
// a 64-bit seed, we suggest to seed a splitmix64 generator and use its
// output to fill s.
// -----------------------------------------------------------------------------
class xoshiro512pp {
 public:
  using result_type = uint64_t;

  explicit xoshiro512pp(const uint64_t seed) {
    SplitMix64 seed_gen(seed);
    for (int i = 0; i < 8; ++i) {
      m_s[i] = seed_gen.next();
    }
  }

  bool equal(const xoshiro512pp &other) const {
    for (int i = 0; i < 8; ++i) {
      if (m_s[i] != other.m_s[i]) return false;
    }
    return true;
  }

  result_type next() {
    const uint64_t result = rotl(m_s[0] + m_s[2], 17) + m_s[2];

    const uint64_t t = m_s[1] << 11;

    m_s[2] ^= m_s[0];
    m_s[5] ^= m_s[1];
    m_s[1] ^= m_s[2];
    m_s[7] ^= m_s[3];
    m_s[3] ^= m_s[4];
    m_s[4] ^= m_s[5];
    m_s[0] ^= m_s[6];
    m_s[6] ^= m_s[7];

    m_s[6] ^= t;

    m_s[7] = rotl(m_s[7], 21);

    return result;
  }

  // This is the jump function for the generator. It is equivalent
  // to 2^256 calls to next(); it can be used to generate 2^256
  // non-overlapping subsequences for parallel computations.
  void jump() {
    static const uint64_t JUMP[] = {0x33ed89b6e7a353f9, 0x760083d7955323be,
                                    0x2837f2fbb5f22fae, 0x4b8c5674d309511c,
                                    0xb11ac47a7ba28c25, 0xf1be7667092bcc1c,
                                    0x53851efdb6df0aaf, 0x1ebbc8b23eaf25db};

    uint64_t t[sizeof m_s / sizeof *m_s];
    memset(t, 0, sizeof t);
    for (int i = 0; i < (int)(sizeof JUMP / sizeof *JUMP); i++)
      for (int b = 0; b < 64; b++) {
        if (JUMP[i] & UINT64_C(1) << b)
          for (int w = 0; w < (int)(sizeof m_s / sizeof *m_s); w++)
            t[w] ^= m_s[w];
        next();
      }

    memcpy(m_s, t, sizeof m_s);
  }

  // This is the long-jump function for the generator. It is equivalent to
  // 2^384 calls to next(); it can be used to generate 2^128 starting points,
  // from each of which jump() will generate 2^128 non-overlapping
  //  subsequences for parallel distributed computations.
  void long_jump() {
    static const uint64_t LONG_JUMP[] = {
        0x11467fef8f921d28, 0xa2a819f2e79c8ea8, 0xa8299fc284b3959a,
        0xb4d347340ca63ee1, 0x1cb0940bedbff6ce, 0xd956c5c4fa1f8e17,
        0x915e38fd4eda93bc, 0x5b3ccdfa5d7daca5};

    uint64_t t[sizeof m_s / sizeof *m_s];
    memset(t, 0, sizeof t);
    for (int i = 0; i < (int)(sizeof LONG_JUMP / sizeof *LONG_JUMP); i++)
      for (int b = 0; b < 64; b++) {
        if (LONG_JUMP[i] & UINT64_C(1) << b)
          for (int w = 0; w < (int)(sizeof m_s / sizeof *m_s); w++)
            t[w] ^= m_s[w];
        next();
      }

    memcpy(m_s, t, sizeof m_s);
  }

 private:
  static constexpr uint64_t rotl(const uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
  }

  uint64_t m_s[8];
};

// -----------------------------------------------------------------------------
// This also contains public domain code from <http://prng.di.unimi.it/>.
// From xoshiro1024plusplus.c:
// -----------------------------------------------------------------------------
// Written in 2019 by David Blackman and Sebastiano Vigna (vigna@acm.org)
//
// To the extent possible under law, the author has dedicated all copyright
// and related and neighboring rights to this software to the public domain
// worldwide. This software is distributed without any warranty.
//
// See <http://creativecommons.org/publicdomain/zero/1.0/>.
//
// This is xoroshiro1024++ 1.0, one of our all-purpose, rock-solid,
// large-state generators. It is extremely fast and it passes all
// tests we are aware of. Its state however is too large--in general,
// the xoshiro256 family should be preferred.
//
// For generating just floating-point numbers, xoroshiro1024* is even
// faster (but it has a very mild bias, see notes in the comments).
//
// The state must be seeded so that it is not everywhere zero. If you have
// a 64-bit seed, we suggest to seed a splitmix64 generator and use its
// output to fill s.
// -----------------------------------------------------------------------------
class xoshiro1024pp {
 public:
  using result_type = uint64_t;

  explicit xoshiro1024pp(const uint64_t seed) {
    SplitMix64 seed_gen(seed);
    for (int i = 0; i < 16; ++i) {
      m_s[i] = seed_gen.next();
    }
  }

  bool equal(const xoshiro1024pp &other) const {
    for (int i = 0; i < 16; ++i) {
      if (m_s[i] != other.m_s[i]) return false;
    }
    return true;
  }

  uint64_t next() {
    const int q = m_p;
    const uint64_t s0 = m_s[m_p = (m_p + 1) & 15];
    uint64_t s15 = m_s[q];
    const uint64_t result = rotl(s0 + s15, 23) + s15;

    s15 ^= s0;
    m_s[q] = rotl(s0, 25) ^ s15 ^ (s15 << 27);
    m_s[m_p] = rotl(s15, 36);

    return result;
  }

  // This is the jump function for the generator. It is equivalent
  // to 2^512 calls to next(); it can be used to generate 2^512
  // non-overlapping subsequences for parallel computations.
  void jump() {
    static const uint64_t JUMP[] = {
        0x931197d8e3177f17, 0xb59422e0b9138c5f, 0xf06a6afb49d668bb,
        0xacb8a6412c8a1401, 0x12304ec85f0b3468, 0xb7dfe7079209891e,
        0x405b7eec77d9eb14, 0x34ead68280c44e4a, 0xe0e4ba3e0ac9e366,
        0x8f46eda8348905b7, 0x328bf4dbad90d6ff, 0xc8fd6fb31c9effc3,
        0xe899d452d4b67652, 0x45f387286ade3205, 0x03864f454a8920bd,
        0xa68fa28725b1b384};

    uint64_t t[sizeof m_s / sizeof *m_s];
    memset(t, 0, sizeof t);
    for (int i = 0; i < (int)(sizeof JUMP / sizeof *JUMP); i++)
      for (int b = 0; b < 64; b++) {
        if (JUMP[i] & UINT64_C(1) << b)
          for (int j = 0; j < int(sizeof m_s / sizeof *m_s); j++)
            t[j] ^= m_s[(j + m_p) & (sizeof m_s / sizeof *m_s - 1)];
        next();
      }

    for (int i = 0; i < (int)(sizeof m_s / sizeof *m_s); i++) {
      m_s[(i + m_p) & (sizeof m_s / sizeof *m_s - 1)] = t[i];
    }
  }

  // This is the long-jump function for the generator. It is equivalent to
  // 2^768 calls to next(); it can be used to generate 2^256 starting points,
  // from each of which jump() will generate 2^256 non-overlapping
  // subsequences for parallel distributed computations.
  void long_jump() {
    static const uint64_t LONG_JUMP[] = {
        0x7374156360bbf00f, 0x4630c2efa3b3c1f6, 0x6654183a892786b1,
        0x94f7bfcbfb0f1661, 0x27d8243d3d13eb2d, 0x9701730f3dfb300f,
        0x2f293baae6f604ad, 0xa661831cb60cd8b6, 0x68280c77d9fe008c,
        0x50554160f5ba9459, 0x2fc20b17ec7b2a9a, 0x49189bbdc8ec9f8f,
        0x92a65bca41852cc1, 0xf46820dd0509c12a, 0x52b00c35fbf92185,
        0x1e5b3b7f589e03c1};

    uint64_t t[sizeof m_s / sizeof *m_s];
    memset(t, 0, sizeof t);
    for (int i = 0; i < (int)(sizeof LONG_JUMP / sizeof *LONG_JUMP); i++)
      for (int b = 0; b < 64; b++) {
        if (LONG_JUMP[i] & UINT64_C(1) << b)
          for (int j = 0; j < (int)(sizeof m_s / sizeof *m_s); j++)
            t[j] ^= m_s[(j + m_p) & (sizeof m_s / sizeof *m_s - 1)];
        next();
      }

    for (int i = 0; i < (int)(sizeof m_s / sizeof *m_s); i++) {
      m_s[(i + m_p) & (sizeof m_s / sizeof *m_s - 1)] = t[i];
    }
  }

 private:
  static constexpr uint64_t rotl(const uint64_t x, const int k) {
    return (x << k) | (x >> (64 - k));
  }

  int m_p{0};
  uint64_t m_s[16];
};

template <typename xoshiro_type>
class base_rand_xoshiro {
 public:
  using result_type = typename xoshiro_type::result_type;

  explicit base_rand_xoshiro(const uint64_t seed = 123)
      : m_rand_generator(seed) {}

  /// \brief Advances the engine's state and returns the generated value.
  /// \return A pseudo-random number in [min(), max()]
  auto operator()() {
    const auto n = m_rand_generator.next();
    assert(min() <= n && n <= max());
    return n;
  }

  /// \brief Compares two pseudo-random number engines.
  /// Two engines are equal, if their internal states are equivalent.
  /// \return Returns true if two engines are equal; otherwise, returns false.
  bool equal(const base_rand_xoshiro &other) {
    return m_rand_generator.equal(other.m_rand_generator);
  }

  /// \brief Gets the smallest possible value in the output range.
  /// \return The minimum value to be potentially generated.
  static constexpr result_type min() {
    return std::numeric_limits<result_type>::min();
  }

  /// \brief Gets the largest possible value in the output range.
  /// \return The maximum value to be potentially generated.
  static constexpr result_type max() {
    return std::numeric_limits<result_type>::max();
  }

  // TODO: implement
  // operator<<() {}
  // operator>>() {}

 private:
  xoshiro_type m_rand_generator;
};

template <typename xoshiro_type>
inline bool operator==(const base_rand_xoshiro<xoshiro_type> &lhs,
                       const base_rand_xoshiro<xoshiro_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename xoshiro_type>
inline bool operator!=(const base_rand_xoshiro<xoshiro_type> &lhs,
                       const base_rand_xoshiro<xoshiro_type> &rhs) {
  return !(lhs == rhs);
}
}  // namespace detail

#endif  // DOXYGEN_SKIP

/// \brief pseudo-random number generator that has a similar interface as the
/// ones in STL The actual algorithm is uses xoshiro512++ whose period is
/// 2^(512-1)
using rand_512 = detail::base_rand_xoshiro<detail::xoshiro512pp>;

/// \brief pseudo-random number generator that has a similar interface as the
/// ones in STL The actual algorithm is uses xoshiro1024++ whose period is
/// 2^(1024-1)
using rand_1024 = detail::base_rand_xoshiro<detail::xoshiro1024pp>;
}  // namespace metall::utility

#endif  // METALL_UTILITY_RANDOM_HPP
