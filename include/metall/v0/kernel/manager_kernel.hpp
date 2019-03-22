// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP
#define METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP

#include <iostream>
#include <cassert>
#include <string>
#include <utility>

#include <metall/offset_ptr.hpp>
#include <metall/v0/kernel/bin_number_manager.hpp>
#include <metall/v0/kernel/bin_directory.hpp>
#include <metall/v0/kernel/chunk_directory.hpp>
#include <metall/v0/kernel/named_object_directory.hpp>
#include <metall/v0/kernel/segment_storage.hpp>
#include <metall/v0/kernel/object_size_manager.hpp>
#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/in_place_interface.hpp>
#include <metall/detail/utility/array_construct.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/char_ptr_holder.hpp>

#define ENABLE_MUTEX_IN_V0_MANAGER_KERNEL 1
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
#include <metall/detail/utility/mutex.hpp>
#endif

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

/// \brief Manager kernel class version 0
/// \tparam chunk_no_type Type of chunk number
/// \tparam k_chunk_size Size of single chunk in byte
template <typename chunk_no_type, std::size_t k_chunk_size>
class manager_kernel {

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using void_pointer = offset_ptr<void>;
  using char_type = char; // required by boost's named proxy
  using char_ptr_holder_type = util::char_ptr_holder<char_type>;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;
  using id_type = uint16_t;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using self_type = manager_kernel<chunk_no_type, k_chunk_size>;

  static constexpr size_type k_max_size = 1ULL << 48; // TODO: get from somewhere else

  // For bin
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  using bin_no_type = typename bin_no_mngr::bin_no_type;
  static constexpr size_type k_num_small_bins = bin_no_mngr::num_small_bins();
  static constexpr const char *k_segment_file_name = "segment";

  // For bin directory (NOTE: we only manage small bins)
  using bin_directory_type = bin_directory<k_num_small_bins, chunk_no_type>;
  static constexpr const char *k_bin_directory_file_name = "bin_directory";

  // For chunk directory
  using chunk_directory_type = chunk_directory<chunk_no_type, k_chunk_size, k_max_size>;
  using chunk_slot_no_type = typename chunk_directory_type::slot_no_type;
  static constexpr const char *k_chunk_directory_file_name = "chunk_directory";

  // For named object directory
  using named_object_directory_type = named_object_directory<difference_type, size_type>;
  static constexpr const char *k_named_object_directory_file_name = "named_object_directory";

  // For data segment manager
  using segment_storage_type = segment_storage<difference_type, size_type>;

  static constexpr size_type k_max_small_object_size = bin_no_mngr::to_object_size(bin_no_mngr::num_small_bins() - 1);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  using mutex_type = util::mutex;
  using lock_guard_type = util::mutex_lock_guard;
#endif

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  manager_kernel() = default;

  ~manager_kernel() {
    close();
  }

  manager_kernel(const manager_kernel &) = delete;
  manager_kernel &operator=(const manager_kernel &) = delete;

  manager_kernel(manager_kernel &&) noexcept = default;
  manager_kernel &operator=(manager_kernel &&) noexcept = default;

 public:
  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Expect to be called by a single thread
  /// \param path
  /// \param nbytes
  void create(const char *path, const size_type nbytes) {
    m_file_base_path = path;

    if (!m_segment_storage.create(priv_file_name(k_segment_file_name).c_str(), nbytes)) {
      std::cerr << "Can not create segment" << std::endl;
      std::abort();
    }

    m_chunk_directory.initialize(priv_num_chunks());
  }

  /// \brief Expect to be called by a single thread
  /// \param path
  /// \return
  bool open(const char *path) {
    m_file_base_path = path;

    if (!m_segment_storage.open(priv_file_name(k_segment_file_name).c_str())) return false;

    m_chunk_directory.initialize(priv_num_chunks());

    if (!m_bin_directory.deserialize(priv_file_name(k_bin_directory_file_name).c_str()) ||
        !m_chunk_directory.deserialize(priv_file_name(k_chunk_directory_file_name).c_str()) ||
        !m_named_object_directory.deserialize(priv_file_name(k_named_object_directory_file_name).c_str())) {
      std::cerr << "Cannot deserialize internal data" << std::endl;
      std::abort();
    }

    return true;
  }

  /// \brief Expect to be called by a single thread
  void close() {
    if (priv_initialized()) {
      sync();
      m_segment_storage.destroy();
    }
  }

  void sync() {
    assert(priv_initialized());

    m_segment_storage.sync();

    if (!m_bin_directory.serialize(priv_file_name(k_bin_directory_file_name).c_str()) ||
        !m_chunk_directory.serialize(priv_file_name(k_chunk_directory_file_name).c_str()) ||
        !m_named_object_directory.serialize(priv_file_name(k_named_object_directory_file_name).c_str())) {
      std::cerr << "Failed to serialize internal data" << std::endl;
      std::abort();
    }
  }

  void *allocate(const size_type nbytes) {
    assert(priv_initialized());

    const bin_no_type bin_no = bin_no_mngr::to_bin_no(nbytes);

    if (bin_no < k_num_small_bins) {
      const size_type object_size = bin_no_mngr::to_object_size(bin_no);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type bin_guard(m_bin_mutex[bin_no]);
#endif

      if (m_bin_directory.empty(bin_no)) {
        chunk_no_type new_chunk_no;
        {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
          lock_guard_type chunk_guard(m_chunk_mutex);
#endif
          new_chunk_no = m_chunk_directory.insert(bin_no);
        }
        m_bin_directory.insert(bin_no, new_chunk_no);
      }

      assert(!m_bin_directory.empty(bin_no));
      const chunk_no_type chunk_no = m_bin_directory.front(bin_no);

      assert(!m_chunk_directory.full_slot(chunk_no));
      const chunk_slot_no_type chunk_slot_no = m_chunk_directory.find_and_mark_slot(chunk_no);

      if (m_chunk_directory.full_slot(chunk_no)) {
        m_bin_directory.pop(bin_no);
      }

      return static_cast<char *>(m_segment_storage.segment()) + k_chunk_size * chunk_no + object_size * chunk_slot_no;

    } else {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type chunk_guard(m_chunk_mutex);
#endif
      const chunk_no_type new_chunk_no = m_chunk_directory.insert(bin_no);
      return static_cast<char *>(m_segment_storage.segment()) + k_chunk_size * new_chunk_no;
    }

    return nullptr;
  }

  // \TODO: implement
  void *allocate_aligned(const size_type nbytes, const size_type alignment) {
    assert(false);
    return nullptr;
  }

  void deallocate(void *addr) {
    assert(priv_initialized());

    if (!addr) return;

    const difference_type offset = static_cast<char *>(addr) - static_cast<char *>(m_segment_storage.segment());
    const chunk_no_type chunk_no = offset / k_chunk_size;
    const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);

    if (bin_no < k_num_small_bins) {
      const size_type object_size = bin_no_mngr::to_object_size(bin_no);
      const auto slot_no = static_cast<chunk_slot_no_type>((offset % k_chunk_size) / object_size);

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type bin_guard(m_bin_mutex[bin_no]);
#endif

      const bool was_full = m_chunk_directory.full_slot(chunk_no);
      m_chunk_directory.unmark_slot(chunk_no, slot_no);
      if (was_full) {
        m_bin_directory.insert(bin_no, chunk_no);
      } else if (m_chunk_directory.empty_slot(chunk_no)) {
        {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
          lock_guard_type chunk_guard(m_chunk_mutex);
#endif
          m_chunk_directory.erase(chunk_no);
          priv_free_chunk(chunk_no, 1);
        }
        m_bin_directory.erase(bin_no, chunk_no);
      }
    } else {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type chunk_guard(m_chunk_mutex);
#endif
      m_chunk_directory.erase(chunk_no);
      const std::size_t num_chunks = bin_no_mngr::to_object_size(bin_no) / k_chunk_size;
      priv_free_chunk(chunk_no, num_chunks);
    }
  }

  template <typename T>
  std::pair<T *, size_type> find(char_ptr_holder_type name) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      return std::make_pair(nullptr, 0);
    }

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
    lock_guard_type guard(m_named_object_directory_mutex); // TODO: don't need at here?
#endif

    const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();

    const auto iterator = m_named_object_directory.find(raw_name);
    if (iterator == m_named_object_directory.end()) {
      return std::make_pair<T *, size_type>(nullptr, 0);
    }

    const auto offset_and_size = iterator->second;
    return std::make_pair(reinterpret_cast<T *>(offset_and_size.first
                              + static_cast<char *>(m_segment_storage.segment())),
                          offset_and_size.second);
  }

  template <typename T>
  bool destroy(char_ptr_holder_type name) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      return false;
    }

    typename named_object_directory_type::mapped_type offset_and_size;
    { // Erase from named_object_directory
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type guard(m_named_object_directory_mutex);
#endif

      const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();

      const auto iterator = m_named_object_directory.find(raw_name);
      if (iterator == m_named_object_directory.end()) return false; // No object with the name
      m_named_object_directory.erase(iterator);
      offset_and_size = iterator->second;
    }

    // Destruct each object
    auto ptr = static_cast<T *>(static_cast<void *>(offset_and_size.first
        + static_cast<char *>(m_segment_storage.segment())));
    for (size_type i = 0; i < offset_and_size.second; ++i) {
      ptr->~T();
      ++ptr;
    }

    deallocate(offset_and_size.first + static_cast<char *>(m_segment_storage.segment()));

    return true;
  }

  /// \brief Generic named/anonymous new function. This method is required by construct_proxy and construct_iter_proxy
  /// \tparam T Type of the object(s)
  /// \param name Name of the object(s)
  /// \param num Number of objects to be constructed
  /// \param try2find If true, tries to find already constructed object(s) with the same name
  /// \param dothrow If true, throws exception
  /// \param table Reference to an in_place_interface object
  /// \return Returns a pointer to the constructed object(s)
  template <typename T>
  T *generic_construct(char_ptr_holder_type name,
                       const size_type num,
                       const bool try2find,
                       const bool dothrow,
                       util::in_place_interface &table) {
    assert(priv_initialized());

    if (name.is_anonymous()) {
      void *const ptr = allocate(num * sizeof(T));
      util::array_construct(ptr, num, table);
      return static_cast<T *>(ptr);
    } else {
      const char *const raw_name = (name.is_unique()) ? typeid(T).name() : name.get();
      return priv_generic_named_construct<T>(raw_name, num, try2find, dothrow, table);
    }
  }

  bool snapshot(const char *base_path) {
    sync();

    if (priv_copy_backing_files(base_path)) {
      return false;
    }

    return true;
  }

  static bool remove_file(const char *base_path) {
    return priv_remove_backing_files(base_path);
  }

  // Implemented in another file.
  void profile(const std::string &log_file_name) const;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  bool priv_initialized() const {
    return (m_segment_storage.segment() && m_segment_storage.size() && !m_file_base_path.empty());
  }

  static std::string priv_file_name(const std::string &base_name, const std::string &name) {
    return base_name + "_" + name;
  }

  std::string priv_file_name(const std::string &name) const {
    return priv_file_name(m_file_base_path, name);
  }

  void priv_free_chunk(const chunk_no_type head_chunk_no, const std::size_t num_chunks) {
    const off_t offset = head_chunk_no * k_chunk_size;
    const size_t length = num_chunks * k_chunk_size;
    m_segment_storage.free_region(offset, length);
  }

  std::size_t priv_num_chunks() const {
    return m_segment_storage.size() / k_chunk_size;
  }

  template <typename T>
  T *priv_generic_named_construct(const char_type *const name,
                                  const size_type num,
                                  const bool try2find,
                                  const bool, // TODO implement 'dothrow'
                                  util::in_place_interface &table) {
    void *ptr = nullptr;
    {
#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
      lock_guard_type guard(m_named_object_directory_mutex); // TODO: implement a better lock strategy
#endif

      const auto iterator = m_named_object_directory.find(name);
      if (iterator != m_named_object_directory.end()) { // Found an entry
        if (try2find) {
          const auto offset_and_size = iterator->second;
          return reinterpret_cast<T *>(offset_and_size.first + static_cast<char *>(m_segment_storage.segment()));
        } else {
          return nullptr;
        }
      }

      ptr = allocate(num * sizeof(T));

      // Insert into named_object_directory
      const auto insert_ret = m_named_object_directory.insert(name,
                                                              static_cast<char *>(ptr)
                                                                  - static_cast<char *>(m_segment_storage.segment()),
                                                              num);
      if (!insert_ret) {
        std::cerr << "Failed to insert a new name: " << name << std::endl;
        return nullptr;
      }
    }

    util::array_construct(ptr, num, table);

    return static_cast<T *>(ptr);
  }

  bool priv_copy_backing_files(const char *const base_name) const {
    if (!util::copy_file(priv_file_name(k_segment_file_name),
                         priv_file_name(base_name, k_segment_file_name))) {
      std::cerr << "Failed to copy a backing file: " << priv_file_name(base_name, k_segment_file_name) << std::endl;
      return false;
    }

    if (!util::copy_file(priv_file_name(k_bin_directory_file_name),
                         priv_file_name(base_name, k_bin_directory_file_name))) {
      std::cerr << "Failed to copy a backing file: " << priv_file_name(base_name, k_bin_directory_file_name)
                << std::endl;
      return false;
    }

    if (!util::copy_file(priv_file_name(k_chunk_directory_file_name),
                         priv_file_name(base_name, k_chunk_directory_file_name))) {
      std::cerr << "Failed to copy a backing file: " << priv_file_name(base_name, k_chunk_directory_file_name)
                << std::endl;
      return false;
    }

    if (!util::copy_file(priv_file_name(k_named_object_directory_file_name),
                         priv_file_name(base_name, k_named_object_directory_file_name))) {
      std::cerr << "Failed to copy a backing file: " << priv_file_name(base_name, k_named_object_directory_file_name)
                << std::endl;
      return false;
    }

    return true;
  }

  static bool priv_remove_backing_files(const char *const base_name) {
    if (!util::remove_file(priv_file_name(base_name, k_segment_file_name))) {
      std::cerr << "Failed to remove a backing file" << std::endl;
      return false;
    }

    if (!util::remove_file(priv_file_name(base_name, k_bin_directory_file_name))) {
      std::cerr << "Failed to remove a backing file" << std::endl;
      return false;
    }

    if (!util::remove_file(priv_file_name(base_name, k_chunk_directory_file_name))) {
      std::cerr << "Failed to remove a backing file" << std::endl;
      return false;
    }

    if (!util::remove_file(priv_file_name(base_name, k_named_object_directory_file_name))) {
      std::cerr << "Failed to remove a backing file" << std::endl;
      return false;
    }

    return true;
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  std::string m_file_base_path;
  bin_directory_type m_bin_directory;
  chunk_directory_type m_chunk_directory;
  named_object_directory_type m_named_object_directory;
  segment_storage_type m_segment_storage;

#if ENABLE_MUTEX_IN_V0_MANAGER_KERNEL
  mutex_type m_chunk_mutex;
  mutex_type m_named_object_directory_mutex;
  std::array<mutex_type, k_num_small_bins> m_bin_mutex;
#endif
};

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_V0_HPP

#include <metall/v0/kernel/manager_kernel_profile_impl.ipp>