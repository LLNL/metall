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
#include <limits>

#include <metall/detail/proc.hpp>
#include <metall/detail/hash.hpp>

#ifndef METALL_DISABLE_CONCURRENCY
#define METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
#endif
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
#include <metall/detail/mutex.hpp>
#endif

// #define METALL_OBJECT_CACHE_HEAVY_DEBUG
#ifdef METALL_OBJECT_CACHE_HEAVY_DEBUG
#warning "METALL_OBJECT_CACHE_HEAVY_DEBUG is defined"
#endif

namespace metall::kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

namespace obcdetail {

/// A cache block contains offsets of cached objects of the same bin (object
/// size). The maximum number of objects in a cache block is 'k_capacity'. Cache
/// blocks compose two double-linked lists: 1) a linked list of cache blocks of
/// the same bin. 2) a linked list of cache blocks of any bin.
template <typename difference_type, typename bin_no_type>
struct cache_block {
  static constexpr unsigned int k_capacity = 64;

  // Disable them to avoid unexpected calls.
  cache_block() = delete;
  ~cache_block() = delete;

  inline void clear() {
    bin_no = std::numeric_limits<bin_no_type>::max();
    older_block = nullptr;
    newer_block = nullptr;
    bin_older_block = nullptr;
    bin_newer_block = nullptr;
  }

  // Remove myself from the linked-lists
  inline void disconnect() {
    if (newer_block) {
      newer_block->older_block = older_block;
    }
    if (older_block) {
      older_block->newer_block = newer_block;
    }
    if (bin_newer_block) {
      bin_newer_block->bin_older_block = bin_older_block;
    }
    if (bin_older_block) {
      bin_older_block->bin_newer_block = bin_newer_block;
    }
  }

  inline void link_to_older(cache_block *const block,
                            cache_block *const bin_block) {
    older_block = block;
    if (block) {
      block->newer_block = this;
    }
    bin_older_block = bin_block;
    if (bin_block) {
      bin_block->bin_newer_block = this;
    }
  }

  bin_no_type bin_no;
  cache_block *older_block;
  cache_block *newer_block;
  cache_block *bin_older_block;
  cache_block *bin_newer_block;
  difference_type cache[k_capacity];
};

/// A bin header contains a pointer to the active block of the corresponding bin
/// and the number of objects in the active block.
/// Inserting and removing objects are done only to the active block. Non-active
/// blocks are always full.
template <typename difference_type, typename bin_no_type>
class bin_header {
 public:
  using cache_block_type = cache_block<difference_type, bin_no_type>;

  bin_header() = default;

  // Move the active block to the next (older) block
  inline void move_to_next_active_block() {
    if (!m_active_block) return;
    m_active_block = m_active_block->bin_older_block;
    m_active_block_size = (m_active_block) ? cache_block_type::k_capacity : 0;
  }

  inline void update_active_block(const cache_block_type *const block,
                                  const std::size_t num_objects) {
    m_active_block = block;
    m_active_block_size = num_objects;
  }

  inline std::size_t &active_block_size() noexcept {
    return m_active_block_size;
  }

  inline std::size_t active_block_size() const noexcept {
    return m_active_block_size;
  }

  inline cache_block_type *active_block() noexcept {
    return const_cast<cache_block_type *>(m_active_block);
  }

  inline const cache_block_type *active_block() const noexcept {
    return m_active_block;
  }

 private:
  // The number of objects in the active block
  std::size_t m_active_block_size{0};
  const cache_block_type *m_active_block{nullptr};
};

/// A free blocks list contains a linked-list of free blocks.
/// This class assumes that A) bocks are located in a contiguous memory region
/// and B) all cache blocks are uninitialized at the beginning so that they do
/// not consume physical memory. This free list is designed such that it does
/// not touch uninitialized blocks until they are used. This design is crucial
/// to reduce Metall manager construction time.
template <typename difference_type, typename bin_no_type>
class free_blocks_list {
 public:
  using cache_block_type = cache_block<difference_type, bin_no_type>;

  free_blocks_list(const cache_block_type *uninit_top, std::size_t num_blocks)
      : m_blocks(nullptr),
        m_uninit_top(uninit_top),
        m_last_block(uninit_top + num_blocks - 1) {
    assert(uninit_top);
    assert(num_blocks > 0);
  }

  inline bool empty() const noexcept { return !m_blocks && !m_uninit_top; }

  // Returns an available free block
  cache_block_type *pop() {
    cache_block_type *block = nullptr;
    if (m_blocks) {
      block = const_cast<cache_block_type *>(m_blocks);
      m_blocks = m_blocks->older_block;
    } else {
      if (m_uninit_top) {
        block = const_cast<cache_block_type *>(m_uninit_top);
        if (m_uninit_top == m_last_block) {
          m_uninit_top = nullptr;
        } else {
          ++m_uninit_top;
        }
      }
    }
    assert(block);
    return block;
  }

  // Push a block to internal block pool
  void push(cache_block_type *block) {
    assert(block);
    block->older_block = const_cast<cache_block_type *>(m_blocks);
    m_blocks = block;
  }

 private:
  // Blocks that were used and became empty
  const cache_block_type *m_blocks;
  // The top block of the uninitialized blocks.
  const cache_block_type *m_uninit_top;
  const cache_block_type *m_last_block;
};

/// A cache header contains some metadata of a single cache.
/// Specifically, it contains the total size (byte) of objects in the cache,
/// the pointers to the oldest and the newest blocks, and a free blocks list.
template <typename difference_type, typename bin_no_type>
struct cache_header {
 private:
  using free_blocks_list_type = free_blocks_list<difference_type, bin_no_type>;

 public:
  using cache_block_type = cache_block<difference_type, bin_no_type>;

  cache_header(const cache_block_type *const blocks, std::size_t num_blocks)
      : m_free_blocks(blocks, num_blocks) {
    assert(blocks);
    assert(num_blocks > 0);
  }

  inline void unregister(const cache_block_type *const block) {
    if (block == m_newest_block) {
      m_newest_block = block->older_block;
    }
    if (block == m_oldest_block) {
      m_oldest_block = block->newer_block;
    }
  }

  inline void register_new_block(const cache_block_type *const block) {
    m_newest_block = block;
    if (!m_oldest_block) {
      m_oldest_block = block;
    }
  }

  inline std::size_t &total_size_byte() noexcept { return m_total_size_byte; }

  inline std::size_t total_size_byte() const noexcept {
    return m_total_size_byte;
  }

  inline cache_block_type *newest_block() noexcept {
    return const_cast<cache_block_type *>(m_newest_block);
  }

  inline const cache_block_type *newest_block() const noexcept {
    return m_newest_block;
  }

  inline cache_block_type *oldest_block() noexcept {
    return const_cast<cache_block_type *>(m_oldest_block);
  }

  inline const cache_block_type *oldest_block() const noexcept {
    return m_oldest_block;
  }

  inline free_blocks_list_type &free_blocks() noexcept { return m_free_blocks; }

  inline const free_blocks_list_type &free_blocks() const noexcept {
    return m_free_blocks;
  }

 private:
  std::size_t m_total_size_byte{0};
  const cache_block_type *m_oldest_block{nullptr};
  const cache_block_type *m_newest_block{nullptr};
  free_blocks_list_type m_free_blocks;
};

/// A cache container contains all data that constitute a cache.
/// This cache container holds a cache header, bin headers, and cache blocks in
/// a contiguous memory region.
template <typename difference_type, typename bin_no_type,
          std::size_t max_bin_no, std::size_t num_blocks_per_cache>
struct cache_container {
  using cache_heaer_type = cache_header<difference_type, bin_no_type>;
  using bin_header_type = bin_header<difference_type, bin_no_type>;
  using cacbe_block_type = cache_block<difference_type, bin_no_type>;

  // Disable the default constructor to avoid unexpected initialization.
  cache_container() = delete;

  // Disable the copy constructor to avoid unexpected destructor call.
  ~cache_container() = delete;

  // This data structure must be initialized first using this function.
  void init() {
    new (&header) cache_heaer_type(blocks, num_blocks_per_cache);
    // Memo: The in-place an array construction may not be supported by some
    // compilers.
    for (std::size_t i = 0; i <= max_bin_no; ++i) {
      new (&bin_headers[i]) bin_header_type();
    }
    // Do not initialize blocks on purpose to reduce the construction time
  }

  // Reset data to the initial state
  void reset_headers() {
    std::destroy_at(&header);
    for (std::size_t i = 0; i <= max_bin_no; ++i) {
      std::destroy_at(&bin_headers[i]);
    }
    init();
  }

  cache_heaer_type header;
  bin_header_type bin_headers[max_bin_no + 1];
  cacbe_block_type blocks[num_blocks_per_cache];
};

// Allocate new objects by this size
template <typename difference_type, typename bin_no_manager>
inline constexpr unsigned int comp_chunk_size(
    const typename bin_no_manager::bin_no_type bin_no) noexcept {
  using cache_block_type =
      cache_block<difference_type, typename bin_no_manager::bin_no_type>;
  const auto object_size = bin_no_manager::to_object_size(bin_no);
  // 4096 is meant for page size so that we do not move memory larger than a
  // page.
  // 8 is meant for the minimum number of objects to cache within a block.
  return std::max(std::min((unsigned int)(4096 / object_size),
                           cache_block_type ::k_capacity),
                  (unsigned int)(8));
}

// Calculate the max bin number that can be cached,
// considering the internal implementation.
template <typename difference_type, typename bin_no_manager>
inline constexpr typename bin_no_manager::bin_no_type comp_max_bin_no(
    const std::size_t max_per_cpu_cache_size,
    const std::size_t max_object_size_request) noexcept {
  constexpr unsigned int k_num_min_chunks_per_bin = 2;

  typename bin_no_manager::bin_no_type b = 0;
  // Support only small bins
  for (; b < bin_no_manager::num_small_bins(); ++b) {
    const auto min_required_cache_size =
        comp_chunk_size<difference_type, bin_no_manager>(b) *
        k_num_min_chunks_per_bin * bin_no_manager::to_object_size(b);

    if (max_per_cpu_cache_size < min_required_cache_size) {
      if (b == 0) {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "The request max per-CPU cache size is too small");
        return 0;
      }
      --b;
      break;
    }
  }

  return std::min(bin_no_manager::to_bin_no(max_object_size_request), b);
}

template <typename difference_type, typename bin_no_manager>
inline constexpr std::size_t comp_max_num_objects_per_cache(
    const std::size_t max_per_cpu_cache_size) noexcept {
  return max_per_cpu_cache_size / bin_no_manager::to_object_size(0);
}

template <typename difference_type, typename bin_no_manager>
inline constexpr std::size_t comp_num_blocks_per_cache(
    const std::size_t max_per_cpu_cache_size) noexcept {
  using cache_block_type =
      cache_block<difference_type, typename bin_no_manager::bin_no_type>;

  return comp_max_num_objects_per_cache<difference_type, bin_no_manager>(
             max_per_cpu_cache_size) /
         cache_block_type::k_capacity;
}

}  // namespace obcdetail

/// A cache for small objects.
/// This class manages per-'CPU' (i.e., CPU-core rather than 'socket') caches
/// internally. Actually stored data is the offsets of cached objects. This
/// cache push and pop objects using a LIFO policy. When the cache is full
/// (exceeds a pre-defined threshold), it deallocates some oldest objects first
/// before caching new ones.
template <typename _size_type, typename _difference_type,
          typename _bin_no_manager, typename _object_allocator_type>
class object_cache {
 public:
  using size_type = _size_type;
  using difference_type = _difference_type;
  using bin_no_manager = _bin_no_manager;
  using bin_no_type = typename bin_no_manager::bin_no_type;
  using object_allocator_type = _object_allocator_type;
  using object_allocate_func_type = void (object_allocator_type:: *const)(
      const bin_no_type, const size_type, difference_type *const);
  using object_deallocate_func_type = void (object_allocator_type:: *const)(
      const bin_no_type, const size_type, const difference_type *const);

 private:
  static constexpr unsigned int k_num_caches_per_cpu =
#ifdef METALL_DISABLE_CONCURRENCY
      1;
#else
      METALL_NUM_CACHES_PER_CPU;
#endif

#ifndef METALL_MAX_PER_CPU_CACHE_SIZE
#error "METALL_MAX_PER_CPU_CACHE_SIZE=byte must be defined"
#endif
  // The actual value is determined, considering other restrictions
  // such as the max object size to cache.
  static constexpr size_type k_max_per_cpu_cache_size =
      METALL_MAX_PER_CPU_CACHE_SIZE;

  // The maximum object size to cache in byte.
  // The actual max object size is determined, considering other restrictions
  // such as the max per-CPU cache size.
  static constexpr size_type k_max_object_size = k_max_per_cpu_cache_size / 16;

  // How long the CPU number is cached.
  static constexpr unsigned int k_cpu_no_cache_duration = 4;

  static constexpr bin_no_type k_max_bin_no =
      obcdetail::comp_max_bin_no<difference_type, bin_no_manager>(
          k_max_per_cpu_cache_size, k_max_object_size);

  static constexpr size_type k_num_blocks_per_cache =
      obcdetail::comp_num_blocks_per_cache<difference_type, bin_no_manager>(
          k_max_per_cpu_cache_size);

#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
  using mutex_type = mdtl::mutex;
  using lock_guard_type = mdtl::mutex_lock_guard;
#endif

  using cache_storage_type =
      obcdetail::cache_container<difference_type, bin_no_type, k_max_bin_no,
                                 k_num_blocks_per_cache>;
  using cache_block_type = typename cache_storage_type::cacbe_block_type;

 public:
  class const_bin_iterator;

  inline static constexpr size_type max_per_cpu_cache_size() noexcept {
    return k_max_per_cpu_cache_size;
  }

  inline static constexpr size_type num_caches_per_cpu() noexcept {
    return k_num_caches_per_cpu;
  }

  inline static constexpr bin_no_type max_bin_no() noexcept {
    return k_max_bin_no;
  }

  explicit object_cache()
      : m_num_caches(priv_get_num_cpus() * k_num_caches_per_cpu)
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
        ,
        m_mutex(m_num_caches)
#endif
  {
    priv_allocate_cache();
  }

  ~object_cache() noexcept = default;
  object_cache(const object_cache &) = default;
  object_cache(object_cache &&) noexcept = default;
  object_cache &operator=(const object_cache &) = default;
  object_cache &operator=(object_cache &&) noexcept = default;

  /// Pop an object offset from the cache.
  /// If the cache is empty, allocate objects and cache them first.
  difference_type pop(const bin_no_type bin_no,
                      object_allocator_type *const allocator_instance,
                      object_allocate_func_type allocator_function,
                      object_deallocate_func_type deallocator_function) {
    return priv_pop(bin_no, allocator_instance, allocator_function,
                    deallocator_function);
  }

  /// Cache an object.
  /// If the cache is full, deallocate some oldest cached objects first.
  /// Return false if an error occurs.
  bool push(const bin_no_type bin_no, const difference_type object_offset,
            object_allocator_type *const allocator_instance,
            object_deallocate_func_type deallocator_function) {
    return priv_push(bin_no, object_offset, allocator_instance,
                     deallocator_function);
  }

  /// Clear all cached objects.
  /// Cached objects are going to be deallocated.
  void clear(object_allocator_type *const allocator_instance,
             object_deallocate_func_type deallocator_function) {
    for (size_type c = 0; c < m_num_caches; ++c) {
      auto &cache = m_cache[c];
      for (bin_no_type b = 0; b <= k_max_bin_no; ++b) {
        auto &bin_header = cache.bin_headers[b];
        cache_block_type *block = bin_header.active_block();
        size_type num_objects = bin_header.active_block_size();
        while (block) {
          if (num_objects > 0) {
            (allocator_instance->*deallocator_function)(b, num_objects,
                                                        block->cache);
          }
          block = block->bin_older_block;
          num_objects = cache_block_type::k_capacity;
        }
      }
      cache.reset_headers();
    }
  }

  inline size_type num_caches() const noexcept { return m_num_caches; }

  const_bin_iterator begin(const size_type cache_no,
                           const bin_no_type bin_no) const {
    assert(cache_no < m_num_caches);
    assert(bin_no <= k_max_bin_no);
    const auto &cache = m_cache[cache_no];
    const auto &bin_header = cache.bin_headers[bin_no];

    if (bin_header.active_block_size() == 0) {
      if (!bin_header.active_block() ||
          !bin_header.active_block()->bin_older_block) {
        // There is no cached objects for the bin.
        return const_bin_iterator();
      }
      // Start from the older block as the active block is empty
      return const_bin_iterator(bin_header.active_block()->bin_older_block,
                                cache_block_type::k_capacity - 1);
    }

    return const_bin_iterator(bin_header.active_block(),
                              bin_header.active_block_size() - 1);
  }

  const_bin_iterator end([[maybe_unused]] const size_type cache_no,
                         [[maybe_unused]] const bin_no_type bin_no) const {
    assert(cache_no < m_num_caches);
    assert(bin_no <= k_max_bin_no);
    return const_bin_iterator();
  }

 private:
  struct free_deleter {
    void operator()(void *const p) const noexcept { std::free(p); }
  };

  inline static unsigned int priv_get_num_cpus() {
    return mdtl::get_num_cpus();
  }

  inline size_type priv_cache_no() const {
#ifdef METALL_DISABLE_CONCURRENCY
    return 0;
#endif
#if SUPPORT_GET_CPU_NO
    thread_local static const auto sub_cache_no =
        std::hash<std::thread::id>{}(std::this_thread::get_id()) %
        k_num_caches_per_cpu;
    return (priv_get_cpu_no() * k_num_caches_per_cpu + sub_cache_no) %
           m_num_caches;
#else
    thread_local static const auto hashed_thread_id = mdtl::hash<>{}(
        std::hash<std::thread::id>{}(std::this_thread::get_id()));
    return hashed_thread_id % m_num_caches;
#endif
  }

  /// Get CPU number.
  /// This function does not call the system call every time as it is slow.
  inline static size_type priv_get_cpu_no() {
#ifdef METALL_DISABLE_CONCURRENCY
    return 0;
#endif
    thread_local static unsigned int cached_cpu_no = 0;
    thread_local static unsigned int cached_count = 0;
    if (cached_cpu_no == 0) {
      cached_cpu_no = mdtl::get_cpu_no();
    }
    cached_count = (cached_count + 1) % k_cpu_no_cache_duration;
    return cached_cpu_no;
  }

  bool priv_allocate_cache() {
    auto *const mem = std::malloc(sizeof(cache_storage_type) * m_num_caches);
    m_cache.reset(reinterpret_cast<cache_storage_type *>(mem));
    if (!m_cache) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to allocate memory for the cache");
      return false;
    }

    for (size_type i = 0; i < m_num_caches; ++i) {
      m_cache[i].init();
    }

    return true;
  }

  difference_type priv_pop(const bin_no_type bin_no,
                           object_allocator_type *const allocator_instance,
                           object_allocate_func_type allocator_function,
                           object_deallocate_func_type deallocator_function) {
    assert(bin_no <= max_bin_no());

    const auto cache_no = priv_cache_no();
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif

    auto &cache = m_cache[cache_no];
    auto &cache_header = cache.header;
    auto &bin_header = cache.bin_headers[bin_no];
    const auto object_size = bin_no_manager::to_object_size(bin_no);

    if (bin_header.active_block_size() == 0) {  // Active block is empty

      if (bin_header.active_block()) {
        // Move to next active block if that is available
        auto *const empty_block = bin_header.active_block();
        bin_header.move_to_next_active_block();
        cache_header.unregister(empty_block);
        empty_block->disconnect();
        cache_header.free_blocks().push(empty_block);
      }

      if (bin_header.active_block_size() == 0) {
        assert(!bin_header.active_block());

        // There is no cached objects for the bin.
        // Allocate some objects and cache them to a free block.
        const auto num_new_objects =
            obcdetail::comp_chunk_size<difference_type, bin_no_manager>(bin_no);
        const auto new_objects_size = num_new_objects * object_size;

        // Make sure that the cache has enough space to allocate objects.
        priv_make_room_for_new_blocks(cache_no, new_objects_size,
                                      allocator_instance, deallocator_function);
        assert(!cache_header.free_blocks().empty());

        // allocate objects to the new block
        auto *new_block = cache_header.free_blocks().pop();
        assert(new_block);
        new_block->clear();
        new_block->bin_no = bin_no;
        (allocator_instance->*allocator_function)(bin_no, num_new_objects,
                                                  new_block->cache);

        // Link the new block to the existing blocks
        new_block->link_to_older(cache_header.newest_block(),
                                 bin_header.active_block());

        // Update headers
        cache_header.register_new_block(new_block);
        cache_header.total_size_byte() += new_objects_size;
        assert(cache_header.total_size_byte() <= k_max_per_cpu_cache_size);
        bin_header.update_active_block(new_block, num_new_objects);
      }
    }
    assert(bin_header.active_block_size() > 0);

    // Pop an object from the active block
    --bin_header.active_block_size();
    const auto object_offset =
        bin_header.active_block()->cache[bin_header.active_block_size()];
    assert(cache_header.total_size_byte() >= object_size);
    cache_header.total_size_byte() -= object_size;
    return object_offset;
  }

  bool priv_push(const bin_no_type bin_no, const difference_type object_offset,
                 object_allocator_type *const allocator_instance,
                 object_deallocate_func_type deallocator_function) {
    assert(object_offset >= 0);
    assert(bin_no <= max_bin_no());

    const auto cache_no = priv_cache_no();
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
    lock_guard_type guard(m_mutex[cache_no]);
#endif

    auto &cache = m_cache[cache_no];
    auto &cache_header = cache.header;
    auto &bin_header = cache.bin_headers[bin_no];
    const auto object_size = bin_no_manager::to_object_size(bin_no);

    // Make sure that the cache has enough space to allocate objects.
    priv_make_room_for_new_blocks(cache_no, object_size, allocator_instance,
                                  deallocator_function);

    if (!bin_header.active_block() ||
        bin_header.active_block_size() == cache_block_type::k_capacity) {
      // There is no cached objects for the bin or
      // the active block is full.
      assert(!cache_header.free_blocks().empty());

      auto *free_block = cache_header.free_blocks().pop();
      assert(free_block);
      free_block->clear();
      free_block->bin_no = bin_no;
      free_block->link_to_older(cache_header.newest_block(),
                                bin_header.active_block());
      cache_header.register_new_block(free_block);
      bin_header.update_active_block(free_block, 0);
    }

    // push an object to the active block
    bin_header.active_block()->cache[bin_header.active_block_size()] =
        object_offset;
    ++bin_header.active_block_size();
    cache_header.total_size_byte() += object_size;
    assert(cache_header.total_size_byte() <= k_max_per_cpu_cache_size);

    return true;
  }

  void priv_make_room_for_new_blocks(
      const size_type cache_no, const size_type new_objects_size,
      object_allocator_type *const allocator_instance,
      object_deallocate_func_type deallocator_function) {
    auto &cache = m_cache[cache_no];
    auto &cache_header = cache.header;
    auto &free_blocks = cache_header.free_blocks();
    auto &bin_headers = cache.bin_headers;
    auto &total_size = cache_header.total_size_byte();

    // Make sure that the cache has enough space to allocate objects.
    while (total_size + new_objects_size > k_max_per_cpu_cache_size ||
           free_blocks.empty()) {
      auto *const oldest_block = cache_header.oldest_block();
      assert(oldest_block);

      // Deallocate objects from the oldest block
      const auto bin_no = oldest_block->bin_no;
      assert(bin_no <= k_max_bin_no);
      auto &bin_header = bin_headers[bin_no];
      const auto object_size = bin_no_manager::to_object_size(bin_no);
      const auto num_objects = (bin_header.active_block() == oldest_block)
                                   ? bin_header.active_block_size()
                                   : cache_block_type::k_capacity;
      (allocator_instance->*deallocator_function)(bin_no, num_objects,
                                                  oldest_block->cache);
      assert(total_size >= num_objects * object_size);
      total_size -= num_objects * object_size;

      cache_header.unregister(oldest_block);
      if (bin_header.active_block() == oldest_block) {
        bin_header.update_active_block(nullptr, 0);
      }
      oldest_block->disconnect();
      free_blocks.push(oldest_block);
    }
  }

  const unsigned int m_num_caches;
#ifdef METALL_ENABLE_MUTEX_IN_OBJECT_CACHE
  std::vector<mutex_type> m_mutex;
#endif
  std::unique_ptr<cache_storage_type[], free_deleter> m_cache{nullptr};
};

/// An iterator to iterate over cached objects of the same bin.
template <typename _size_type, typename _difference_type,
          typename _bin_no_manager, typename _object_allocator_type>
class object_cache<_size_type, _difference_type, _bin_no_manager,
                   _object_allocator_type>::const_bin_iterator {
 public:
  using value_type = difference_type;
  using pointer = const value_type *;
  using reference = const value_type &;

  const_bin_iterator() noexcept = default;  // set to the end

  const_bin_iterator(const cache_block_type *const block,
                     const unsigned int in_block_pos) noexcept
      : m_block(block), m_in_block_pos(in_block_pos) {
    assert(m_block || m_in_block_pos == 0);
  }

  const_bin_iterator(const const_bin_iterator &) = default;
  const_bin_iterator(const_bin_iterator &&) noexcept = default;
  const_bin_iterator &operator=(const const_bin_iterator &) = default;
  const_bin_iterator &operator=(const_bin_iterator &&) noexcept = default;

  reference operator*() const noexcept {
    return m_block->cache[m_in_block_pos];
  }

  pointer operator->() const noexcept {
    return &m_block->cache[m_in_block_pos];
  }

  const_bin_iterator &operator++() noexcept {
    if (m_in_block_pos == 0) {
      m_block = m_block->bin_older_block;
      if (!m_block) {
        return *this;
      }
      m_in_block_pos = cache_block_type::k_capacity - 1;
    } else {
      --m_in_block_pos;
    }

    return *this;
  }

  const_bin_iterator operator++(int) noexcept {
    const auto tmp = *this;
    ++(*this);
    return tmp;
  }

  bool operator==(const const_bin_iterator &other) const noexcept {
    return m_block == other.m_block && m_in_block_pos == other.m_in_block_pos;
  }

  bool operator!=(const const_bin_iterator &other) const noexcept {
    return !(*this == other);
  }

 private:
  const cache_block_type *m_block{nullptr};
  unsigned int m_in_block_pos{0};
};
}  // namespace metall::kernel
#endif  // METALL_DETAIL_OBJECT_CACHE_HPP
