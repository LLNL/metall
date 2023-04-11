// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_OBJECT_CACHE_HPP
#define METALL_DETAIL_OBJECT_CACHE_HPP

#include <iostream>
#include <cassert>
#include <mutex>
#include <vector>
#include <thread>
#include <memory>

#include <boost/container/vector.hpp>

#include <metall/kernel/object_cache_container.hpp>
#include <metall/detail/proc.hpp>
#include <metall/detail/hash.hpp>
#define ENABLE_MUTEX_IN_METALL_OBJECT_CACHE 1
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
#include <metall/detail/mutex.hpp>
#endif

namespace metall::kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

template <std::size_t _k_num_bins, typename _size_type,
          typename _difference_type, typename _bin_no_manager,
          typename _object_allocator_type>
class object_cache {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  static constexpr unsigned int k_num_bins = _k_num_bins;
  using size_type = _size_type;
  using difference_type = _difference_type;
  using bin_no_manager = _bin_no_manager;
  using bin_no_type = typename bin_no_manager::bin_no_type;
  using object_allocator_type = _object_allocator_type;
  using object_allocate_func_type = void (object_allocator_type::*const)(
      const bin_no_type, const size_type, difference_type *const);
  using object_deallocate_func_type = void (object_allocator_type::*const)(
      const bin_no_type, const size_type, const difference_type *const);

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //

  static constexpr std::size_t k_num_cache_per_core = 4;
  static constexpr std::size_t k_cache_bin_size = 1ULL << 20ULL;
  static constexpr std::size_t k_max_cache_block_size =
      64;  // Add and remove caches by up to this size
  static constexpr std::size_t k_max_cache_object_size =
      k_cache_bin_size / k_max_cache_block_size / 2;
  static constexpr bin_no_type k_max_bin_no =
      bin_no_manager::to_bin_no(k_max_cache_object_size);
  static constexpr std::size_t k_cpu_core_no_cache_duration = 4;

  using single_cache_type =
      object_cache_container<k_cache_bin_size, k_max_bin_no + 1,
                             difference_type, bin_no_manager>;
  using cache_table_type = std::vector<single_cache_type>;

#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
  using mutex_type = mdtl::mutex;
  using lock_guard_type = mdtl::mutex_lock_guard;
#endif

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using const_bin_iterator = typename single_cache_type::const_iterator;

  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  object_cache()
      : m_cache_table(get_num_cores() * k_num_cache_per_core)
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
        ,
        m_mutex(m_cache_table.size())
#endif
  {
  }

  ~object_cache() noexcept = default;
  object_cache(const object_cache &) = default;
  object_cache(object_cache &&) noexcept = default;
  object_cache &operator=(const object_cache &) = default;
  object_cache &operator=(object_cache &&) noexcept = default;

  // -------------------- //
  // Public methods
  // -------------------- //

  /// \brief
  /// \param bin_no
  /// \return
  difference_type pop(const bin_no_type bin_no,
                      object_allocator_type *const allocator_instance,
                      object_allocate_func_type allocator_function) {
    if (bin_no > max_bin_no()) return -1;

    const auto cache_no = priv_comp_cache_no();
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    if (m_cache_table[cache_no].empty(bin_no)) {
      difference_type allocated_offsets[k_max_cache_block_size];
      const auto block_size = priv_get_cache_block_size(bin_no);
      (allocator_instance->*allocator_function)(bin_no, block_size,
                                                allocated_offsets);
      for (std::size_t i = 0; i < block_size; ++i) {
        m_cache_table[cache_no].push(bin_no, allocated_offsets[i]);
      }
    }

    const auto offset = m_cache_table[cache_no].front(bin_no);
    m_cache_table[cache_no].pop(bin_no);
    return offset;
  }

  /// \brief
  /// \param bin_no
  /// \param object_offset
  bool push(const bin_no_type bin_no, const difference_type object_offset,
            object_allocator_type *const allocator_instance,
            object_deallocate_func_type deallocator_function) {
    assert(object_offset >= 0);
    if (bin_no > max_bin_no()) return false;  // Error

    const auto cache_no = priv_comp_cache_no();
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    m_cache_table[cache_no].push(bin_no, object_offset);

    if (m_cache_table[cache_no].full(bin_no)) {
      const auto block_size = priv_get_cache_block_size(bin_no);
      assert(m_cache_table[cache_no].size(bin_no) >= block_size);
      difference_type offsets[k_max_cache_object_size];
      for (std::size_t i = 0; i < block_size; ++i) {
        offsets[i] = m_cache_table[cache_no].front(bin_no);
        m_cache_table[cache_no].pop(bin_no);
      }
      (allocator_instance->*deallocator_function)(bin_no, block_size, offsets);
    }

    return true;
  }

  void clear() {
    for (auto &table : m_cache_table) {
      table.clear();
    }
  }

  std::size_t num_caches() const { return m_cache_table.size(); }

  /// \brief The max bin number this cache manages.
  static constexpr bin_no_type max_bin_no() { return k_max_bin_no; }

  const_bin_iterator begin(const std::size_t cache_no,
                           const bin_no_type bin_no) const {
    return m_cache_table[cache_no].begin(bin_no);
  }

  const_bin_iterator end(const std::size_t cache_no,
                         const bin_no_type bin_no) const {
    return m_cache_table[cache_no].end(bin_no);
  }

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //

  // -------------------- //
  // Private methods
  // -------------------- //
  static constexpr std::size_t priv_get_cache_block_size(
      const bin_no_type bin_no) noexcept {
    const auto object_size = bin_no_manager::to_object_size(bin_no);
    // Returns a value on the interval [8, k_max_cache_block_size].
    return std::max(std::min(4096 / object_size, k_max_cache_block_size),
                    static_cast<std::size_t>(8));
  }

  std::size_t priv_comp_cache_no() const {
#if SUPPORT_GET_CPU_CORE_NO
    thread_local static const auto sub_cache_no =
        std::hash<std::thread::id>{}(std::this_thread::get_id()) %
        k_num_cache_per_core;
    const std::size_t core_num = priv_get_core_no();
    return mdtl::hash<std::size_t>{}(core_num * k_num_cache_per_core +
                                     sub_cache_no) %
           m_cache_table.size();
#else
    thread_local static const auto hashed_thread_id = mdtl::hash<std::size_t>{}(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return hashed_thread_id % m_cache_table.size();
#endif
  }

  /// \brief Get CPU core number.
  /// This function does not call the system call every time as it is slow.
  static std::size_t priv_get_core_no() {
    thread_local static int cached_core_no = 0;
    thread_local static int cached_count = 0;
    if (cached_core_no == 0) {
      cached_core_no = mdtl::get_cpu_core_no();
    }
    cached_count = (cached_count + 1) % k_cpu_core_no_cache_duration;
    return cached_core_no;
  }

  static std::size_t get_num_cores() {
    return std::thread::hardware_concurrency();
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  cache_table_type m_cache_table;
#if ENABLE_MUTEX_IN_METALL_OBJECT_CACHE
  std::vector<mutex_type> m_mutex;
#endif
};

}  // namespace metall::kernel
#endif  // METALL_DETAIL_OBJECT_CACHE_HPP
