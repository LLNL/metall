// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_OBJECT_CACHE_CONTAINER_HPP
#define METALL_DETAIL_OBJECT_CACHE_CONTAINER_HPP

#include <array>
#include <vector>
#include <numeric>

namespace metall::kernel {

/// \brief Namespace for object cache details.
namespace objccdetail {

/// \brief Computes the capacity table at compile time.
template <std::size_t k_num_bins, std::size_t k_bin_size, typename bin_no_manager>
inline constexpr auto init_capacity_table() noexcept {
  // MEMO: {} is needed to prevent the uninitialized error in constexpr contexts.
  // This technique is not needed from C++20.
  std::array<std::size_t, k_num_bins> capacity_table{};
  for (std::size_t b = 0; b < capacity_table.size(); ++b) {
    capacity_table[b] = k_bin_size / bin_no_manager::to_object_size(b);
  }
  return capacity_table;
}

/// \brief Holds the capacity of each bin of an object cache.
template <std::size_t k_num_bins, std::size_t k_bin_size, typename bin_no_manager>
constexpr std::array<std::size_t, k_num_bins>
    capacity_table = init_capacity_table<k_num_bins, k_bin_size, bin_no_manager>();

/// \brief Computes the offset table at compile time.
template <std::size_t k_num_bins, std::size_t k_bin_size, typename bin_no_manager>
inline constexpr auto init_offset_table() noexcept {
  std::array<std::size_t, k_num_bins> offset_table{};
  offset_table[0] = 0;
  for (std::size_t b = 0; b < offset_table.size() - 1; ++b) {
    offset_table[b + 1] = offset_table[b] + capacity_table<k_num_bins, k_bin_size, bin_no_manager>[b];
  }
  return offset_table;
}

/// \brief Holds the offset to the beginning of each bin in the object cache
/// (bins are packed as an 1D array).
template <std::size_t k_num_bins, std::size_t k_bin_size, typename bin_no_manager>
constexpr std::array<std::size_t, k_num_bins>
    offset_table = init_offset_table<k_num_bins, k_bin_size, bin_no_manager>();

/// \brief Computes the maximum number of items an object cache can holds.
template <std::size_t k_num_bins, std::size_t k_bin_size, typename bin_no_manager>
inline constexpr auto compute_capacity() noexcept {
  std::size_t capacity = 0;
  for (const auto n: capacity_table<k_num_bins, k_bin_size, bin_no_manager>) {
    capacity += n;
  }
  return capacity;
}

} // namespace objccdetail

/// \brief Container for object cache.
/// This container is designed to hold the offset addresses of cached objects.
/// \tparam _k_bin_size The total cached object size per bin.
/// \tparam _k_num_bins The number of bins.
/// \tparam _difference_type The address offset type.
/// \tparam _bin_no_manager The bin number manager.
template <std::size_t _k_bin_size,
    std::size_t _k_num_bins, typename _difference_type, typename _bin_no_manager>
class object_cache_container {
 public:
  static constexpr std::size_t k_bin_size = _k_bin_size;
  static constexpr std::size_t k_num_bins = _k_num_bins;
  using difference_type = _difference_type;
  using bin_no_manager = _bin_no_manager;
  using bin_no_type = typename bin_no_manager::bin_no_type;

 private:
  using cache_table_type = std::vector<difference_type>;

  static constexpr auto capacity_table = objccdetail::capacity_table<k_num_bins, k_bin_size, bin_no_manager>;
  static constexpr auto offset_table = objccdetail::offset_table<k_num_bins, k_bin_size, bin_no_manager>;
  static constexpr auto k_cache_capacity = objccdetail::compute_capacity<k_num_bins, k_bin_size, bin_no_manager>();

 public:
  using const_iterator = typename cache_table_type::const_iterator;

  object_cache_container() : m_count_table(k_num_bins, 0), m_cache(k_cache_capacity) {}

  bool push(const bin_no_type bin_no, const difference_type object_offset) {
    if (full(bin_no)) {
      return false; // Error
    }
    const std::size_t pos = offset_table[bin_no] + m_count_table[bin_no];
    assert(pos < m_cache.size());
    m_cache[pos] = object_offset;
    ++m_count_table[bin_no];

    return true;
  }

  difference_type front(const bin_no_type bin_no) const {
    if (empty(bin_no)) return -1;
    const std::size_t pos = offset_table[bin_no] + m_count_table[bin_no] - 1;
    assert(pos < m_cache.size());
    return m_cache[pos];
  }

  bool pop(const bin_no_type bin_no) {
    if (empty(bin_no) || m_count_table[bin_no] == 0) return false; // Error
    --m_count_table[bin_no];
    return true;
  }

  const_iterator begin(const bin_no_type bin_no) const {
    assert(bin_no < m_cache.size());
    const std::size_t pos = offset_table[bin_no];
    return m_cache.begin() + pos;
  }

  const_iterator end(const bin_no_type bin_no) const {
    assert(bin_no < m_cache.size());
    const std::size_t pos = offset_table[bin_no] + m_count_table[bin_no];
    return m_cache.begin() + pos;
  }

  std::size_t size(const bin_no_type bin_no) const {
    return m_count_table[bin_no];
  }

  bool empty(const bin_no_type bin_no) const {
    return m_count_table[bin_no] == 0;
  }

  bool full(const bin_no_type bin_no) const {
    return m_count_table[bin_no] == capacity_table[bin_no];
  }

  void clear() {
    for (auto& n : m_count_table) {
      n = 0;
    }
  }

 private:
  /// \brief Holds the number of cached objects for each bin.
  std::vector<std::size_t> m_count_table;
  /// \brief The container that actually holds the offsets of cached objects.
  cache_table_type m_cache;
};

}

#endif //METALL_DETAIL_OBJECT_CACHE_CONTAINER_HPP
