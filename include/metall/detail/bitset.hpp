// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_BITSET_HPP
#define METALL_DETAIL_UTILITY_BITSET_HPP

#include <type_traits>
#include <cstdint>
#include <limits>
#include <array>
#include <cassert>
#include <vector>

#include <metall/detail/utilities.hpp>

namespace metall::mtlldetail {
namespace bitset_detail {

// example (sizeof(block_type) is 8 byte)
// input 0 ~ 63 -> return 0; input 64 ~ 127 -> return 1;
template <typename block_type>
inline constexpr uint64_t global_index(const uint64_t idx) noexcept {
  return (idx >> log_cpt(sizeof(block_type) * 8ULL, 2));
}

template <typename block_type>
inline constexpr uint64_t local_index(const uint64_t idx) noexcept {
  return idx & (sizeof(block_type) * 8 - 1);
}

// \brief select the best underling type for bitset based on the number of bits
// needed \example 0 ~ 8   bits -> uint8_t 9 ~ 16  bits -> uint16_t 17 ~ 32 bits
// -> uint32_t 33~     bits -> uint64_t
template <uint64_t num_bits>
struct block_type {
  using type = typename std::conditional<
      num_bits <= sizeof(uint8_t) * 8, uint8_t,
      typename std::conditional<
          num_bits <= sizeof(uint16_t) * 8, uint16_t,
          typename std::conditional<num_bits <= sizeof(uint32_t) * 8, uint32_t,
                                    uint64_t>::type>::type>::type;
};

// examples: block_type = uint64_t
// input 1 ~ 64 -> return 1;  input 65 ~ 128 -> return 2
template <typename block_type>
inline constexpr uint64_t num_blocks(const uint64_t num_bits) noexcept {
  return (num_bits == 0)
             ? 0
             : (num_bits - 1ULL) / (sizeof(block_type) * 8ULL) + 1ULL;
}

template <typename block_type>
inline bool full_block(const block_type bitset) noexcept {
  return static_cast<block_type>(bitset + static_cast<block_type>(1)) == 0;
}

template <typename block_type>
inline bool empty_block(const block_type bitset) noexcept {
  return bitset == 0;
}

template <typename block_type>
inline bool get(const block_type *const bitset, const uint64_t idx) noexcept {
  const block_type mask = (0x1ULL << (sizeof(block_type) * 8ULL -
                                      local_index<block_type>(idx) - 1));
  return (bitset[global_index<block_type>(idx)] & mask);
}

template <typename block_type>
inline void set(block_type *const bitset, const uint64_t idx) noexcept {
  const block_type mask = (0x1ULL << (sizeof(block_type) * 8ULL -
                                      local_index<block_type>(idx) - 1));
  bitset[global_index<block_type>(idx)] |= mask;
}

template <typename block_type>
inline void fill(block_type *const bitset) noexcept {
  *bitset = ~static_cast<block_type>(0);
}

template <typename block_type>
inline void erase(block_type *const bitset) noexcept {
  *bitset = static_cast<block_type>(0);
}

template <typename block_type>
inline void reset(block_type *const bitset, const uint64_t idx) noexcept {
  const block_type mask = (0x1ULL << (sizeof(block_type) * 8ULL -
                                      local_index<block_type>(idx) - 1));
  bitset[global_index<block_type>(idx)] &= ~mask;
}

template <uint64_t local_num_bits>
inline typename block_type<local_num_bits>::type generate_mask(
    const uint64_t start_idx, const uint64_t n) noexcept {
  using block_type = typename block_type<local_num_bits>::type;
  const auto x = (start_idx == 0) ? ~static_cast<block_type>(0)
                                  : (static_cast<block_type>(1)
                                     << (local_num_bits - start_idx)) -
                                        static_cast<block_type>(1);
  const auto tail_mask =
      static_cast<block_type>(static_cast<block_type>(1)
                              << (local_num_bits - start_idx - n)) -
      1;
  // std::cout << start_idx << ", " << n << " : " << x << " " << ~tail_mask << "
  // -> " << (x & ~tail_mask) << std::endl;
  return static_cast<block_type>(x & ~tail_mask);
}

// set_mode == true -> set mode
// set_mode == false -> reset mode
template <typename block_type>
inline void update_n_bits(block_type *const bitset, const uint64_t start_idx,
                          const uint64_t n, const bool set_mode) noexcept {
  constexpr uint64_t block_type_num_bits = sizeof(block_type) * 8ULL;
  if (local_index<block_type>(start_idx) + n <=
      block_type_num_bits) {  // update a single block
    const auto mask = generate_mask<block_type_num_bits>(
        local_index<block_type>(start_idx), n);
    if (set_mode)
      bitset[global_index<block_type>(start_idx)] |= mask;
    else
      bitset[global_index<block_type>(start_idx)] &= ~mask;
  } else {  // update multiple blocks
    {       // head block
      const auto local_idx = local_index<block_type>(start_idx);
      const auto mask = generate_mask<block_type_num_bits>(
          local_idx, block_type_num_bits - local_idx);
      // std::cout << "head " << global_index<block_type>(start_idx) << " " <<
      // local_idx << " " << mask << std::endl;
      if (set_mode)
        bitset[global_index<block_type>(start_idx)] |= mask;
      else
        bitset[global_index<block_type>(start_idx)] &= ~mask;
    }

    {  // update blocks in middle
      const uint64_t num_blocks_in_total =
          global_index<block_type>(start_idx + n - 1) -
          global_index<block_type>(start_idx) + 1;
      // std::cout << "middle " << num_blocks_in_total << std::endl;
      for (uint64_t i = 0; i < num_blocks_in_total - 2; ++i) {
        if (set_mode)
          bitset[i + global_index<block_type>(start_idx) + 1] =
              ~static_cast<block_type>(0);
        else
          bitset[i + global_index<block_type>(start_idx) + 1] =
              static_cast<block_type>(0);
      }
    }

    {  // update tail block
      const uint64_t num_bits_to_fill =
          local_index<block_type>(start_idx + n - 1) + 1;
      const auto mask = generate_mask<block_type_num_bits>(0, num_bits_to_fill);
      // std::cout << "tail " << global_index<block_type>(start_idx + n - 1) <<
      // " " << num_bits_to_fill << " " << mask << std::endl;
      if (set_mode)
        bitset[global_index<block_type>(start_idx + n - 1)] |= mask;
      else
        bitset[global_index<block_type>(start_idx + n - 1)] &= ~mask;
    }
  }
}

}  // namespace bitset_detail

template <uint64_t _num_bit>
class static_bitset {
 public:
  static constexpr uint64_t num_bit = _num_bit;
  using block_type = typename bitset_detail::block_type<num_bit>::type;
  static constexpr uint64_t num_local_bit = sizeof(block_type) * 8;

 private:
  static constexpr uint64_t k_num_block =
      bitset_detail::num_blocks<block_type>(num_bit);
  using internal_table_t = std::array<block_type, k_num_block>;

 public:
  using iterator = typename internal_table_t::iterator;
  using const_iterator = typename internal_table_t::const_iterator;

 public:
  static_bitset() : m_table() { m_table.fill(static_cast<block_type>(0)); }

  static_bitset(const static_bitset &) = default;
  static_bitset(static_bitset &&) = default;

  static_bitset &operator=(const static_bitset &rhs) = default;
  static_bitset &operator=(static_bitset &&rhs) = default;

  // TODO: test
  bool operator==(const static_bitset &other) const {
    for (uint64_t i = 0; i < k_num_block; ++i) {
      if (m_table[i] != other.m_table[i]) return false;
    }
    return true;
  }

  // TODO: test
  bool operator!=(const static_bitset &other) const {
    return !((*this) == other);
  }

  // TODO: test
  const static_bitset &operator&=(const static_bitset &rhs) {
    for (uint64_t i = 0; i < k_num_block; ++i) {
      m_table[i] &= rhs.m_table[i];
    }
    return (*this);
  }

  // TODO: test
  const static_bitset &operator|=(const static_bitset &rhs) {
    for (uint64_t i = 0; i < k_num_block; ++i) {
      m_table[i] |= rhs.m_table[i];
    }
    return (*this);
  }

  iterator begin() { return m_table.begin(); }

  iterator end() { return m_table.end(); }

  const_iterator cbegin() const { return m_table.cbegin(); }

  const_iterator cend() const { return m_table.cend(); }

  static uint64_t size() { return num_bit; }

  bool get(const uint64_t idx) const {
    return bitset_detail::get(m_table.data(), idx);
  }

  void set(const uint64_t idx) { bitset_detail::set(m_table.data(), idx); }

  void reset(const uint64_t idx) { bitset_detail::reset(m_table.data(), idx); }

  // TODO: implement
  void flip([[maybe_unused]] const uint64_t idx) { assert(false); }

  void set_n_bits(const uint64_t idx, const uint64_t n) {
    bitset_detail::update_n_bits<block_type>(m_table.data(), idx, n, true);
  }

  void reset_n_bits(const uint64_t idx, const uint64_t n) {
    bitset_detail::update_n_bits<block_type>(m_table.data(), idx, n, false);
  }

  bool any() const {
    for (uint64_t i = 0; i < k_num_block; ++i) {
      if (m_table[i]) return true;
    }
    return false;
  }

 private:
  internal_table_t m_table;
} __attribute__((packed));

template <std::size_t _num_max_bit>
class bitset {
 public:
  static constexpr std::size_t num_max_bit = _num_max_bit;
  using block_type = typename bitset_detail::block_type<num_max_bit>::type;
  static constexpr uint64_t num_local_bit = sizeof(block_type) * 8;

 private:
  using internal_table_t = std::vector<block_type>;

 public:
  using iterator = typename internal_table_t::iterator;
  using const_iterator = typename internal_table_t::const_iterator;

 public:
  bitset() : m_table() {}

  bitset(const std::size_t num_bit) : m_table(num_bit, 0) {}

  ~bitset() = default;

  bitset(const bitset &) = default;
  bitset(bitset &&) = default;

  bitset &operator=(const bitset &rhs) = default;
  bitset &operator=(bitset &&rhs) = default;

  // TODO: test
  bool operator==(const bitset &other) const {
    for (uint64_t i = 0; i < m_table.size(); ++i) {
      if (m_table[i] != other.m_table[i]) return false;
    }
    return true;
  }

  // TODO: test
  bool operator!=(const bitset &other) const { return !((*this) == other); }

  // TODO: test
  const bitset &operator&=(const bitset &rhs) {
    for (uint64_t i = 0; i < m_table.size(); ++i) {
      m_table[i] &= rhs.m_table[i];
    }
    return (*this);
  }

  // TODO: test
  const bitset &operator|=(const bitset &rhs) {
    for (uint64_t i = 0; i < m_table.size(); ++i) {
      m_table[i] |= rhs.m_table[i];
    }
    return (*this);
  }

  iterator begin() { return m_table.begin(); }

  iterator end() { return m_table.end(); }

  const_iterator cbegin() const { return m_table.cbegin(); }

  const_iterator cend() const { return m_table.cend(); }

  uint64_t size() const { return m_table.size(); }

  void resize(const std::size_t num_bit) {
    m_table.resize(bitset_detail::num_blocks<block_type>(num_bit), 0);
  }

  void reset_all() { std::fill(m_table.begin(), m_table.end(), 0); }

  bool get(const uint64_t idx) const {
    return bitset_detail::get(m_table.data(), idx);
  }

  void set(const uint64_t idx) { bitset_detail::set(m_table.data(), idx); }

  void reset(const uint64_t idx) { bitset_detail::reset(m_table.data(), idx); }

  block_type get_block(const uint64_t idx) const {
    return m_table[bitset_detail::global_index<block_type>(idx)];
  }

  bool full_block(const uint64_t idx) const {
    return m_table[bitset_detail::global_index<block_type>(idx)] ==
           ~static_cast<block_type>(0);
  }

  // TODO: implement
  void flip([[maybe_unused]] const uint64_t idx) { assert(false); }

  void set_n_bits(const uint64_t idx, const uint64_t n) {
    bitset_detail::update_n_bits<block_type>(m_table.data(), idx, n, true);
  }

  void reset_n_bits(const uint64_t idx, const uint64_t n) {
    bitset_detail::update_n_bits<block_type>(m_table.data(), idx, n, false);
  }

  bool any() const {
    for (uint64_t i = 0; i < m_table.size(); ++i) {
      if (m_table[i]) return true;
    }
    return false;
  }

 private:
  internal_table_t m_table;
};

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_BITSET_HPP
