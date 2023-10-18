// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
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
#include <sstream>

#include <boost/container/vector.hpp>

#include <metall/kernel/object_cache_container.hpp>
#include <metall/detail/proc.hpp>
#include <metall/detail/hash.hpp>

#ifndef METALL_DISABLE_CONCURRENCY
#define METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
#endif
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
#include <metall/detail/mutex.hpp>
#endif

namespace metall::kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

template <typename _size_type,
          typename _difference_type, typename _bin_no_manager,
          typename _object_allocator_type>
class object_cache {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
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

  static constexpr std::size_t k_num_cache_per_cpu =
#ifdef METALL_DISABLE_CONCURRENCY
      1;
#else
      METALL_NUM_CACHES_PER_CPU;
#endif

  // The size of each cache bin in bytes.
  static constexpr std::size_t k_cache_bin_size = METALL_CACHE_BIN_SIZE;
  // Add and remove cache objects with this number of objects at a time.
  static constexpr std::size_t k_max_num_objects_in_block = 64;
  // The maximum object size to cache in byte.
  static constexpr std::size_t k_max_cache_object_size =
      k_cache_bin_size / k_max_num_objects_in_block / 2;
  static constexpr bin_no_type k_max_bin_no =
      bin_no_manager::to_bin_no(k_max_cache_object_size);
  static constexpr std::size_t k_cpu_no_cache_duration = 4;

  using single_cache_type =
      object_cache_container<k_cache_bin_size, k_max_bin_no + 1,
                             difference_type, bin_no_manager>;
  using cache_table_type = std::vector<single_cache_type>;

#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
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
      : m_cache_table(mdtl::get_num_cpus() * k_num_cache_per_cpu)
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
        ,
        m_mutex(m_cache_table.size())
#endif
  {
    priv_const_helper();
  }

  ~object_cache() noexcept = default;
  object_cache(const object_cache &) = default;
  object_cache(object_cache &&) noexcept = default;
  object_cache &operator=(const object_cache &) = default;
  object_cache &operator=(object_cache &&) noexcept = default;

  /// \brief Pop an object offset from the cache.
  difference_type pop(const bin_no_type bin_no,
                      object_allocator_type *const allocator_instance,
                      object_allocate_func_type allocator_function) {
    if (bin_no > max_bin_no()) return -1;

    const auto cache_no = priv_comp_cache_no();
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    // If the cache is empty, allocate objects
    if (m_cache_table[cache_no].empty(bin_no)) {
      difference_type allocated_offsets[k_max_num_objects_in_block];
      const auto block_size = priv_get_cache_block_size(bin_no);
      assert(block_size <= k_max_num_objects_in_block);
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

  /// \brief Cache an object.
  /// If the cache is full, multiple objects are deallocated at a time.
  /// Return false if an error occurs.
  bool push(const bin_no_type bin_no, const difference_type object_offset,
            object_allocator_type *const allocator_instance,
            object_deallocate_func_type deallocator_function) {
    assert(object_offset >= 0);
    if (bin_no > max_bin_no()) return false;  // Error

    const auto cache_no = priv_comp_cache_no();
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif
    m_cache_table[cache_no].push(bin_no, object_offset);

    // If the cache is full, deallocate objects
    if (m_cache_table[cache_no].full(bin_no)) {
      const auto block_size = priv_get_cache_block_size(bin_no);
      assert(m_cache_table[cache_no].size(bin_no) >= block_size);
      difference_type offsets[k_max_num_objects_in_block];
      assert(block_size <= k_max_num_objects_in_block);
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
    // Returns a value on the interval [8, k_max_num_objects_in_block].
    return std::max(std::min(4096 / object_size, k_max_num_objects_in_block),
                    static_cast<std::size_t>(8));
  }

  void priv_const_helper() {
    if (mdtl::get_num_cpus() == 0) {
      logger::out(logger::level::critical, __FILE__, __LINE__,
                  "The achieved number of cpus is zero");
      return;
    }
    {
      std::stringstream ss;
      ss << "The number of cpus: " << mdtl::get_num_cpus();
      logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());
    }
    {
      std::stringstream ss;
      ss << "#of caches: " << m_cache_table.size();
      logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());
    }
    {
      std::stringstream ss;
      ss << "Cache capacity per bin: ";
      for (std::size_t b = 0; b < single_cache_type::num_bins(); ++b) {
        ss << single_cache_type::bin_capacity(b);
        if (b < single_cache_type::num_bins() - 1) ss << " ";
      }
      logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());
    }
  }

  std::size_t priv_comp_cache_no() const {
#ifdef METALL_DISABLE_CONCURRENCY
    return 0;
#endif
#if SUPPORT_GET_CPU_NO
    thread_local static const auto sub_cache_no =
        std::hash<std::thread::id>{}(std::this_thread::get_id()) %
        k_num_cache_per_cpu;
    const std::size_t cpu_no = priv_get_cpu_no();
    return mdtl::hash<>{}(cpu_no * k_num_cache_per_cpu + sub_cache_no) %
           m_cache_table.size();
#else
    thread_local static const auto hashed_thread_id = mdtl::hash<>{}(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return hashed_thread_id % m_cache_table.size();
#endif
  }

  /// \brief Get CPU number.
  /// This function does not call the system call every time as it is slow.
  static std::size_t priv_get_cpu_no() {
#ifdef METALL_DISABLE_CONCURRENCY
    return 0;
#endif
    thread_local static int cached_cpu_no = 0;
    thread_local static int cached_count = 0;
    if (cached_cpu_no == 0) {
      cached_cpu_no = mdtl::get_cpu_no();
    }
    cached_count = (cached_count + 1) % k_cpu_no_cache_duration;
    return cached_cpu_no;
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  cache_table_type m_cache_table;
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
  std::vector<mutex_type> m_mutex;
#endif
};

}  // namespace metall::kernel
#endif  // METALL_DETAIL_OBJECT_CACHE_HPP
