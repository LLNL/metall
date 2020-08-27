// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_COMMON_HPP
#define METALL_DETAIL_UTILITY_COMMON_HPP

#include <cstdint>
#include <type_traits>
#include <limits>
#include <cassert>
#include <algorithm>

#include <metall/detail/utility/builtin_functions.hpp>

namespace metall::detail::utility {

/// \brief Computes the next power of 2
/// \param n Input value
/// \return Returns the next power of 2
inline constexpr uint64_t next_power_of_2(const uint64_t n) noexcept {
  uint64_t x(n);

  x--;
  x |= x >> 1ULL;
  x |= x >> 2ULL;
  x |= x >> 4ULL;
  x |= x >> 8ULL;
  x |= x >> 16ULL;
  x |= x >> 32ULL;
  x++;

  return x;
}

/// \brief Rounds up to the nearest multiple of 'base'.
/// base must not be 0.
/// \param to_round
/// \param base
/// \return
inline constexpr int64_t round_up(const int64_t to_round, const int64_t base) noexcept {
  return ((to_round + static_cast<int64_t>(to_round >= 0) * (base - 1)) / base) * base;
}

/// \brief Rounds down to the nearest multiple of 'base'.
/// base must not be 0.
/// \param to_round
/// \param base
/// \return
inline constexpr int64_t round_down(const int64_t to_round, const int64_t base) noexcept {
  return ((to_round - static_cast<int64_t>(to_round < 0) * (base - 1)) / base) * base;
}

/// \brief Computes the base 'base' logarithm of 'n' at compile time.
/// NOTE that this method causes a recursive function call if non-constexpr value is given
/// \param n Input value
/// \param base Base
/// \return Returns the base 'base' logarithm of 'n'
inline constexpr uint64_t log_cpt(const uint64_t n, const uint64_t base) noexcept {
  return (n < base) ? 0 : 1 + log_cpt(n / base, base);
}

/// \brief Compute log2 using a builtin function in order to avoid loop/recursive operation happen in utility::log.
/// If 'n' is 0 or not a power of 2, the result is invalid.
/// \param n Input value
/// \return Returns base 2 logarithm of 2
inline uint64_t log2_dynamic(const uint64_t n) noexcept {
  assert(n && !(n & (n - 1))); // n > 0 && a power of 2
  return ctzll(n);
}

/// \brief Computes the 'exp'-th power of base.
/// NOTE that this method causes a recursive function call if non-constexpr value is given
/// \param base Base
/// \param exp Exponent
/// \return Returns the 'exp'-th power of base.
inline constexpr uint64_t power_cpt(const uint64_t base, const uint64_t exp) noexcept {
  return (exp == 0) ? 1 : base * power_cpt(base, exp - 1);
}

/// \brief Find the proper primitive variable type to store 'x'
/// \tparam x Input value. Must be x <= std::numeric_limits<uint64_t>::max().
template <uint64_t x>
struct unsigned_variable_type {
 private:
  template <bool _cond, typename _iftrue, typename _iffalse>
  using conditional = std::conditional<_cond, _iftrue, _iffalse>;

  template <typename type>
  using limits = std::numeric_limits<type>;

 public:
  using type  =
  typename conditional<x <= limits<uint8_t>::max(),
                       uint8_t,
                       typename conditional<x <= limits<uint16_t>::max(),
                                            uint16_t,
                                            typename conditional<x <= limits<uint32_t>::max(),
                                                                 uint32_t,
                                                                 typename conditional<x <= limits<uint64_t>::max(),
                                                                                      uint64_t,
                                                                                      void>::type>::type>::type>::type;
};

/// \brief
/// \param length
/// \param my_no
/// \param num_sub_groups
/// \return The begin and end index of the sub-group. Note that [begin, end)
std::pair<std::size_t, std::size_t> partial_range(const std::size_t length,
                                                  const std::size_t my_no,
                                                  const std::size_t num_sub_groups) {
  std::size_t partial_length = length / num_sub_groups;
  std::size_t r = length % num_sub_groups;

  std::size_t begin_index;

  if (my_no < r) {
    begin_index = (partial_length + 1) * my_no;
    ++partial_length;
  } else {
    begin_index = (partial_length + 1) * r + partial_length * (my_no - r);
  }

  return std::make_pair(begin_index, begin_index + partial_length);
}
} // namespace metall::detail::utility
#endif //METALL_DETAIL_UTILITY_COMMON_HPP
