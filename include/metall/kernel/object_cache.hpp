// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_OBJECT_CACHE_HPP
#define METALL_DETAIL_OBJECT_CACHE_HPP

#include <iostream>
#include <cassert>
#include <mutex>
#include <vector>
#include <thread>
#include <boost/container/vector.hpp>
#include <metall/kernel/bin_directory.hpp>
#include <metall/detail/utility/proc.hpp>
#include <metall_utility/hash.hpp>
#define ENABLE_MUTEX_IN_METALL_OBJECT_CACHE 1
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
#include <metall/detail/utility/mutex.hpp>
#endif

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <std::size_t _k_num_bins, typename _difference_type, typename bin_no_mngr, typename _allocator_type>
class object_cache {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  static constexpr unsigned int k_num_bins = _k_num_bins;
  static constexpr unsigned int k_full_cache_size = 8;
  using difference_type = _difference_type;
  using allocator_type = _allocator_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;
  using local_object_cache_type = bin_directory<_k_num_bins, difference_type, allocator_type>;
  using cache_table_allocator = boost::container::scoped_allocator_adaptor<other_allocator_type<local_object_cache_type>>;
  using cache_table_type = boost::container::vector<local_object_cache_type, cache_table_allocator>;

  static constexpr unsigned int k_num_cache_per_core = 4;
  static constexpr std::size_t k_max_total_cache_size_per_bin = 1ULL << 20ULL;
  static constexpr unsigned int k_cache_block_size = 8; // Add and remove caches by this size
  static constexpr std::size_t k_max_cache_object_size = k_max_total_cache_size_per_bin / k_cache_block_size / 2;
  static constexpr unsigned int k_max_bin_no = bin_no_mngr::to_bin_no(k_max_cache_object_size);
  static constexpr int k_cpu_core_no_cache_duration = 4;

#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using bin_no_type = typename local_object_cache_type::bin_no_type;
  using const_bin_iterator = typename local_object_cache_type::const_bin_iterator;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  object_cache(const allocator_type &allocator)
      : m_cache_table(num_cores() * k_num_cache_per_core, allocator)
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
  , m_mutex(m_cache_table.size())
#endif
  {};

  ~object_cache() = default;
  object_cache(const object_cache &) = default;
  object_cache(object_cache &&) = default;
  object_cache &operator=(const object_cache &) = default;
  object_cache &operator=(object_cache &&) = default;

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //

  /// \brief
  /// \param bin_no
  /// \param allocator
  /// \return
  difference_type get(const bin_no_type bin_no,
                      const std::function<void(bin_no_type, unsigned int, _difference_type *const)> &allocator) {
    if (bin_no > max_bin_no()) return -1;

    const auto cache_no = comp_cache_no();
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    if (m_cache_table[cache_no].empty(bin_no)) {
      difference_type allocated_offsets[k_cache_block_size];
      allocator(bin_no, k_cache_block_size, allocated_offsets);
      for (unsigned int i = 0; i < k_cache_block_size; ++i) {
        m_cache_table[cache_no].insert(bin_no, allocated_offsets[i]);
      }
    }

    const auto offset = m_cache_table[cache_no].front(bin_no);
    m_cache_table[cache_no].pop(bin_no);
    return offset;
  }

  /// \brief
  /// \param bin_no
  /// \param object_offset
  bool insert(const bin_no_type bin_no, const difference_type object_offset,
              const std::function<void(bin_no_type, unsigned int, const _difference_type *const)> &deallocator) {
    assert(object_offset >= 0);
    if (bin_no > max_bin_no()) return false; // Error

    const auto cache_no = comp_cache_no();
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    m_cache_table[cache_no].insert(bin_no, object_offset);

    const auto object_size = bin_no_mngr::to_object_size(bin_no);
    if (m_cache_table[cache_no].size(bin_no) * object_size >= k_max_total_cache_size_per_bin) {
      assert(m_cache_table[cache_no].size(bin_no) >= k_cache_block_size);
      difference_type offsets[k_cache_block_size];
      for (unsigned int i = 0; i < k_cache_block_size; ++i) {
        offsets[i] = m_cache_table[cache_no].front(bin_no);
        m_cache_table[cache_no].pop(bin_no);
      }
      deallocator(bin_no, k_cache_block_size, offsets);
    }

    return true;
  }

  void clear() {
    for (auto& table : m_cache_table) {
      table.clear();
    }
  }

  unsigned int num_caches() const {
    return m_cache_table.size();
  }

  static constexpr unsigned int max_bin_no() {
    return k_max_bin_no;
  }

  const_bin_iterator begin(const unsigned int cache_no, const bin_no_type bin_no) const {
    return m_cache_table[cache_no].begin(bin_no);
  }

  const_bin_iterator end(const unsigned int cache_no, const bin_no_type bin_no) const {
    return m_cache_table[cache_no].end(bin_no);
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  unsigned int comp_cache_no() const {
#if SUPPORT_GET_CPU_CORE_NO
    thread_local static const auto sub_cache_no = std::hash<std::thread::id>{}(std::this_thread::get_id()) % k_num_cache_per_core;
    const unsigned int core_num = get_core_no();
    return metall::utility::hash<unsigned int>{}(core_num * k_num_cache_per_core + sub_cache_no) % m_cache_table.size();
#else
    thread_local static const auto hashed_thread_id = metall::utility::hash<unsigned int>{}(std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return hashed_thread_id % m_cache_table.size();
#endif
  }

  /// \brief Get CPU core number
  /// Does not call the system call every time as it is slow
  static unsigned int get_core_no() {
    thread_local static int cached_core_no = 0;
    thread_local static int cached_count = 0;
    if (cached_core_no == 0) {
      cached_core_no = util::get_cpu_core_no();
    }
    cached_count = (cached_count + 1) % k_cpu_core_no_cache_duration;
    return cached_core_no;
  }

  static unsigned int num_cores() {
    return std::thread::hardware_concurrency();
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  cache_table_type m_cache_table;
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
  std::vector<mutex_type> m_mutex;
#endif
};

} // namespace kernel
} // namespace metall
#endif //METALL_DETAIL_OBJECT_CACHE_HPP
