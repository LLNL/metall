// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_STATIC_BITSET_HPP
#define METALL_DETAIL_UTILITY_STATIC_BITSET_HPP

#include <type_traits>
#include <cstdint>
#include <limits>
#include <array>

#include <metall/detail/utilities.hpp>

namespace metall::mtlldetail {

namespace bitset_detail {

// example (sizeof(bitset_base_type) is 8 byte)
// input 0 ~ 63 -> return 0; input 64 ~ 127 -> return 1;
template <typename bitset_base_type>
inline constexpr std::size_t bitset_global_pos(const std::size_t pos) {
  return (pos >> cal_log2(sizeof(bitset_base_type) * 8ULL));
}

template <typename bitset_base_type>
inline constexpr std::size_t bitset_local_pos(const std::size_t pos) {
  return pos & (sizeof(bitset_base_type) * 8 - 1);
}

// \brief select the best underling type for bitset based on the number of bits
// needed \example 0 ~ 8   bits -> uint8_t 9 ~ 16  bits -> uint16_t 17 ~ 32 bits
// -> uint32_t 33~     bits -> uint64_t
template <std::size_t num_bits>
using bitset_base_type = typename std::conditional<
    num_bits <= 8, uint8_t,
    typename std::conditional<
        num_bits <= 16, uint16_t,
        typename std::conditional<num_bits <= 32, uint32_t,
                                  uint64_t>::type>::type>::type;

// examples: bitset_base_type = uint64_t
// input 1 ~ 64 -> return 1;  input 65 ~ 128 -> return 2
template <typename bitset_base_type>
inline constexpr std::size_t bitset_size(const std::size_t size) {
  return (size == 0) ? 0
                     : (size - 1ULL) / (sizeof(bitset_base_type) * 8ULL) + 1ULL;
}

template <typename bitset_base_type>
inline bool get_bit(const bitset_base_type *const bitset,
                    const std::size_t pos) {
  const bitset_base_type mask =
      (0x1ULL << (sizeof(bitset_base_type) * 8ULL -
                  bitset_local_pos<bitset_base_type>(pos) - 1));
  return (bitset[bitset_global_pos<bitset_base_type>(pos)] & mask);
}

template <typename bitset_base_type>
inline void set_bit(bitset_base_type *const bitset, const std::size_t pos) {
  const bitset_base_type mask =
      (0x1ULL << (sizeof(bitset_base_type) * 8ULL -
                  bitset_local_pos<bitset_base_type>(pos) - 1));
  bitset[bitset_global_pos<bitset_base_type>(pos)] |= mask;
}

template <typename bitset_base_type>
inline void reset_bit(bitset_base_type *const bitset, const std::size_t pos) {
  const bitset_base_type mask =
      (0x1ULL << (sizeof(bitset_base_type) * 8ULL -
                  bitset_local_pos<bitset_base_type>(pos) - 1));
  bitset[bitset_global_pos<bitset_base_type>(pos)] &= ~mask;
}

template <std::size_t local_num_bits>
inline bitset_base_type<local_num_bits> fill_bits_local(
    const std::size_t start_pos, const std::size_t n) {
  using base_type = bitset_base_type<local_num_bits>;
  const auto x = (start_pos == 0) ? ~static_cast<base_type>(0)
                                  : (static_cast<base_type>(1)
                                     << (local_num_bits - start_pos)) -
                                        static_cast<base_type>(1);
  const auto tail_mask =
      static_cast<base_type>(static_cast<base_type>(1)
                             << (local_num_bits - start_pos - n)) -
      1;
  // std::cout << start_pos << ", " << n << " : " << x << " " << ~tail_mask << "
  // -> " << (x & ~tail_mask) << std::endl;
  return static_cast<base_type>(x & ~tail_mask);
}

// set_mode == true -> set mode
// set_mode == false -> reset mode
template <typename bitset_base_type>
inline void update_n_bits(bitset_base_type *const bitset,
                          const std::size_t start_pos, const std::size_t n,
                          const bool set_mode) {
  constexpr std::size_t base_type_num_bits = sizeof(bitset_base_type) * 8ULL;
  if (bitset_local_pos<bitset_base_type>(start_pos) + n <=
      base_type_num_bits) {  // update a single bin
    const auto mask = fill_bits_local<base_type_num_bits>(
        bitset_local_pos<bitset_base_type>(start_pos), n);
    if (set_mode)
      bitset[bitset_global_pos<bitset_base_type>(start_pos)] |= mask;
    else
      bitset[bitset_global_pos<bitset_base_type>(start_pos)] &= ~mask;
  } else {  // update multiple bins
    {       // head bin
      const auto local_pos = bitset_local_pos<bitset_base_type>(start_pos);
      const auto mask = fill_bits_local<base_type_num_bits>(
          local_pos, base_type_num_bits - local_pos);
      // std::cout << "head " << bitset_global_pos<bitset_base_type>(start_pos)
      // << " " << local_pos << " " << mask << std::endl;
      if (set_mode)
        bitset[bitset_global_pos<bitset_base_type>(start_pos)] |= mask;
      else
        bitset[bitset_global_pos<bitset_base_type>(start_pos)] &= ~mask;
    }

    {  // update bins in middle
      const std::size_t num_bins_in_total =
          bitset_global_pos<bitset_base_type>(start_pos + n - 1) -
          bitset_global_pos<bitset_base_type>(start_pos) + 1;
      // std::cout << "middle " << num_bins_in_total << std::endl;
      for (std::size_t i = 0; i < num_bins_in_total - 2; ++i) {
        if (set_mode)
          bitset[i + bitset_global_pos<bitset_base_type>(start_pos) + 1] =
              ~static_cast<bitset_base_type>(0);
        else
          bitset[i + bitset_global_pos<bitset_base_type>(start_pos) + 1] =
              static_cast<bitset_base_type>(0);
      }
    }

    {  // update tail bin
      const std::size_t num_bits_to_fill =
          bitset_local_pos<bitset_base_type>(start_pos + n - 1) + 1;
      const auto mask =
          fill_bits_local<base_type_num_bits>(0, num_bits_to_fill);
      // std::cout << "tail " << bitset_global_pos<bitset_base_type>(start_pos +
      // n - 1) << " " << num_bits_to_fill << " " << mask << std::endl;
      if (set_mode)
        bitset[bitset_global_pos<bitset_base_type>(start_pos + n - 1)] |= mask;
      else
        bitset[bitset_global_pos<bitset_base_type>(start_pos + n - 1)] &= ~mask;
    }
  }
}

}  // namespace bitset_detail

template <std::size_t _num_bit>
class static_bitset {
 public:
  static constexpr std::size_t num_bit = _num_bit;
  using base_type = bitset_detail::bitset_base_type<num_bit>;
  static constexpr std::size_t num_local_bit = sizeof(base_type) * 8;

 private:
  static constexpr std::size_t k_num_bin =
      bitset_detail::bitset_size<base_type>(num_bit);
  using internal_table_t = std::array<base_type, k_num_bin>;

 public:
  using iterator = typename internal_table_t::iterator;
  using const_iterator = typename internal_table_t::const_iterator;

 public:
  static_bitset<_num_bit>() { m_table.fill(0); };

  static_bitset<_num_bit>(const static_bitset<_num_bit> &) = default;
  static_bitset<_num_bit>(static_bitset<_num_bit> &&) = default;

  static_bitset<_num_bit> &operator=(const static_bitset &rhs) = default;
  static_bitset<_num_bit> &operator=(static_bitset &&rhs) = default;

  // TODO: test
  bool operator==(const static_bitset<_num_bit> &other) const {
    for (std::size_t i = 0; i < k_num_bin; ++i) {
      if (m_table[i] != other.m_table[i]) return false;
    }
    return true;
  }

  // TODO: test
  bool operator!=(const static_bitset<_num_bit> &other) const {
    return !((*this) == other);
  }

  // TODO: test
  const static_bitset<_num_bit> &operator&=(const static_bitset &rhs) {
    for (std::size_t i = 0; i < k_num_bin; ++i) {
      m_table[i] &= rhs.m_table[i];
    }
    return (*this);
  }

  // TODO: test
  const static_bitset<_num_bit> &operator|=(const static_bitset &rhs) {
    for (std::size_t i = 0; i < k_num_bin; ++i) {
      m_table[i] |= rhs.m_table[i];
    }
    return (*this);
  }

  iterator begin() { return m_table.begin(); }

  iterator end() { return m_table.end(); }

  const_iterator cbegin() const { return m_table.cbegin(); }

  const_iterator cend() const { return m_table.cend(); }

  static std::size_t size() { return num_bit; }

  bool get(const std::size_t pos) const {
    return bitset_detail::get_bit(m_table.data(), pos);
  }

  void set(const std::size_t pos) {
    bitset_detail::set_bit(m_table.data(), pos);
  }

  void reset(const std::size_t pos) {
    bitset_detail::reset_bit(m_table.data(), pos);
  }

  // TODO: implement
  void flip(const std::size_t pos) { assert(false); }

  void set_n_bits(const std::size_t pos, const std::size_t n) {
    bitset_detail::update_n_bits<base_type>(m_table.data(), pos, n, true);
  }

  void reset_n_bits(const std::size_t pos, const std::size_t n) {
    bitset_detail::update_n_bits<base_type>(m_table.data(), pos, n, false);
  }

  bool any() const {
    for (std::size_t i = 0; i < k_num_bin; ++i) {
      if (m_table[i]) return true;
    }
    return false;
  }

 private:
  internal_table_t m_table;
} __attribute__((packed));

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_STATIC_BITSET_HPP
