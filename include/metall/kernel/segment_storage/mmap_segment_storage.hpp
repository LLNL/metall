// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_SEGMENT_STORAGE_MMAP_SEGMENT_STORAGE_HPP
#define METALL_KERNEL_SEGMENT_STORAGE_MMAP_SEGMENT_STORAGE_HPP

#include <string>
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/detail/utility/common.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

/// \brief Segment storage that uses multiple backing files
/// The current implementation does not delete files even though that are empty
template <typename different_type, typename size_type>
class mmap_segment_storage {
 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  mmap_segment_storage()
      : m_system_page_size(0),
        m_num_blocks(0),
        m_vm_region_size(0),
        m_current_segment_size(0),
        m_segment(nullptr),
        m_base_path(),
        m_read_only(),
        m_free_file_space(true),
        m_block_fd_list(),
        m_block_size(0) {
#ifdef METALL_USE_PRIVATE_MAP_AND_MSYNC
    logger::out(logger::level::info, __FILE__, __LINE__, "METALL_USE_PRIVATE_MAP_AND_MSYNC is defined");
#endif
#ifdef METALL_USE_PRIVATE_MAP_AND_PWRITE
    logger::out(logger::level::info, __FILE__, __LINE__, "METALL_USE_PRIVATE_MAP_AND_PWRITE is defined");
#endif

    priv_load_system_page_size();
  }

  ~mmap_segment_storage() {
    sync(true);
    destroy();
  }

  mmap_segment_storage(const mmap_segment_storage &) = delete;
  mmap_segment_storage &operator=(const mmap_segment_storage &) = delete;

  mmap_segment_storage(mmap_segment_storage &&other) noexcept:
      m_system_page_size(other.m_system_page_size),
      m_num_blocks(other.m_num_blocks),
      m_vm_region_size(other.m_vm_region_size),
      m_current_segment_size(other.m_current_segment_size),
      m_segment(other.m_segment),
      m_base_path(other.m_base_path),
      m_read_only(other.m_read_only),
      m_free_file_space(other.m_free_file_space),
      m_block_fd_list(std::move(other.m_block_fd_list)),
      m_block_size(other.m_block_size) {
    other.priv_reset();
  }

  mmap_segment_storage &operator=(mmap_segment_storage &&other) noexcept {
    m_system_page_size = other.m_system_page_size;
    m_num_blocks = other.m_num_blocks;
    m_vm_region_size = other.m_vm_region_size;
    m_current_segment_size = other.m_current_segment_size;
    m_segment = other.m_segment;
    m_base_path = other.m_base_path;
    m_read_only = other.m_read_only;
    m_free_file_space = other.m_free_file_space;
    m_block_fd_list = std::move(other.m_block_fd_list);
    m_block_size = other.m_block_size;

    other.priv_reset();

    return (*this);
  }

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Checks if there is a file that can be opened
  static bool openable(const std::string &base_path) {
    const auto file_name = priv_make_file_name(base_path, 0);
    return util::file_exist(file_name);
  }

  /// \brief Gets the size of an existing segment.
  /// This is a static version of size() method.
  static size_type get_size(const std::string &base_path) {
    int block_no = 0;
    size_type total_file_size = 0;
    while (true) {
      const auto file_name = priv_make_file_name(base_path, block_no);
      if (!util::file_exist(file_name)) {
        break;
      }
      total_file_size += util::get_file_size(file_name);
      ++block_no;
    }
    return total_file_size;
  }

  /// \brief Creates a new segment
  bool create(const std::string &base_path,
              const size_type vm_region_size,
              void *const vm_region,
              const size_type block_size) {
    assert(!priv_inited());
    m_block_size = std::min(vm_region_size, block_size);

    logger::out(logger::level::info, __FILE__, __LINE__, "Create a file with prefix " + base_path);

    // TODO: align those values to the page size instead of aborting
    if (m_block_size % page_size() != 0 || vm_region_size % page_size() != 0
        || (uint64_t)vm_region % page_size() != 0) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Invalid argument to crete application data segment");
      return false;
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = false;

    if (!priv_create_and_map_file(m_base_path, 0, m_block_size, 0)) {
      priv_reset();
      return false;
    }
    m_current_segment_size = m_block_size;
    m_num_blocks = 1;

    priv_test_file_space_free(base_path);

    return true;
  }

  /// \brief Opens an existing segment
  bool open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    assert(!priv_inited());

    logger::out(logger::level::info, __FILE__, __LINE__, "Open a file with prefix " + base_path);

    // TODO: align those values to the page size instead of aborting
    if (vm_region_size % page_size() != 0 || (uint64_t)vm_region % page_size() != 0) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Invalid argument to open segment");
      return false;
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = read_only;

    m_num_blocks = 0;
    while (true) {
      const auto file_name = priv_make_file_name(m_base_path, m_num_blocks);
      if (!util::file_exist(file_name)) {
        break;
      }

      const auto file_size = util::get_file_size(file_name);
      assert(file_size % page_size() == 0);
      if (m_block_size > 0 && m_block_size != (size_type)file_size) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "File sizes are not the same");
        return false;
      }
      m_block_size = file_size;

      if (!priv_map_file(file_name,
                         m_block_size,
                         m_current_segment_size,
                         read_only)) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to map a file " + std::to_string(m_block_size));
        return false;
      }
      m_current_segment_size += m_block_size;
      ++m_num_blocks;
    }

    if (!read_only) {
      priv_test_file_space_free(base_path);
    }

    return m_num_blocks > 0;
  }

  /// \brief Extends the currently opened segment if necessary
  /// \param request_size A segment size to extend to
  /// \return Returns true if the segment is extended to or already larger than the requested size.
  /// Returns false on failure.
  bool extend(const size_type request_size) {
    assert(priv_inited());

    if (m_read_only) {
      return false;
    }

    if (request_size > m_vm_region_size) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Requested segment size is bigger than the reserved VM size");
      return false;
    }

    if (request_size <= m_current_segment_size) {
      return true; // Already has enough segment size
    }

    while (m_current_segment_size < request_size) {
      if (!priv_create_and_map_file(m_base_path,
                                    m_num_blocks,
                                    m_block_size,
                                    m_current_segment_size)) {
        priv_reset();
        return false;
      }
      ++m_num_blocks;
      m_current_segment_size += m_block_size;
    }

    return true;
  }

  /// \brief Destroys the segment --- the data will be lost.
  /// To save data to files, sync() must be called beforehand.
  void destroy() {
    priv_destroy_segment();
  }

  /// \brief Syncs the segment with backing files.
  /// \param sync If false is specified, this function returns before finishing the sync operation.
  /// This parameter is ignored for the private map mode.
  void sync(const bool sync) {
    priv_sync_segment(sync);
  }

  /// \brief Tries to free the specified region in DRAM and storage layers.
  /// The actual behavior depends on the system running on.
  /// \param offset An offset to the region from the beginning of the segment.
  /// \param nbytes The size of the region.
  void free_region(const different_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  /// \brief Returns the beginning address of the segment.
  void *get_segment() const {
    return m_segment;
  }

  size_type size() const {
    return m_current_segment_size;
  }

  size_type page_size() const {
    return m_system_page_size;
  }

  bool read_only() const {
    return m_read_only;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  static std::string priv_make_file_name(const std::string &base_path, const size_type n) {
    return base_path + "_block-" + std::to_string(n);
  }

  void priv_reset() {
    m_system_page_size = 0;
    m_num_blocks = 0;
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_segment = nullptr;
    // m_read_only = false;
  }

  bool priv_inited() const {
    return (m_system_page_size > 0 && m_num_blocks > 0 && m_vm_region_size > 0 && m_current_segment_size > 0
        && m_segment && !m_base_path.empty());
  }

  bool priv_create_and_map_file(const std::string &base_path,
                                const size_type block_number,
                                const size_type file_size,
                                const different_type segment_offset) {
    const std::string file_name = priv_make_file_name(base_path, block_number);
    logger::out(logger::level::info, __FILE__, __LINE__,
                "Create and extend a file " + file_name + " with " + std::to_string(file_size) + " bytes");

    if (!util::create_file(file_name)) return false;
    if (!util::extend_file_size(file_name, file_size)) return false;
    if (static_cast<size_type>(util::get_file_size(file_name)) < file_size) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to create and extend file: " + file_name);
      return false;
    }

    if (!priv_map_file(file_name, file_size, segment_offset, false)) {
      return false;
    }
    return true;
  }

  bool priv_map_file(const std::string &path,
                     const size_type file_size,
                     const different_type segment_offset,
                     const bool read_only) {
    assert(!path.empty());
    assert(file_size > 0);
    assert(segment_offset >= 0);
    assert(segment_offset + file_size <= m_vm_region_size);

    [[maybe_unused]] static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif

    const auto map_addr = static_cast<char *>(m_segment) + segment_offset;
    logger::out(logger::level::info, __FILE__, __LINE__,
                "Map a file " + path + " at " + std::to_string(segment_offset) +
                    " with " + std::to_string(file_size) + " bytes; read-only mode is " + std::to_string(read_only));

    const auto ret = (read_only) ?
                     util::map_file_read_mode(path, map_addr, file_size, 0, MAP_FIXED) :
                     #if (METALL_USE_PRIVATE_MAP_AND_MSYNC || METALL_USE_PRIVATE_MAP_AND_PWRITE)
                     util::map_file_write_private_mode(path, map_addr, file_size, 0, MAP_FIXED);
#else
    util::map_file_write_mode(path, map_addr, file_size, 0, MAP_FIXED | map_nosync);
#endif
    if (ret.first == -1 || !ret.second) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to map a file: " + path);
      if (ret.first == -1) {
        util::os_close(ret.first);
      }
      return false;
    }

    m_block_fd_list.emplace_back(ret.first);

    return true;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    for (const auto &fd : m_block_fd_list) {
      util::os_close(fd);
    }

    // Destroy the mapping region by calling mmap with PROT_NONE over the region.
    // As the unmap system call syncs the data first, this approach is significantly fast.
    util::map_with_prot_none(m_segment, m_current_segment_size);
    // NOTE: the VM region will be unmapped by manager_kernel

    priv_reset();
  }

  void priv_sync_segment([[maybe_unused]] const bool sync) {
    if (!priv_inited() || m_read_only) return;

    // Protect the region to detect unexpected write by application during msync
    if (!util::mprotect_read_only(m_segment, m_current_segment_size)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to protect the segment with the read only mode");
      return;
    }

#if METALL_USE_PRIVATE_MAP_AND_MSYNC
    logger::out(logger::level::info, __FILE__, __LINE__, "diff-msync for the application data segment");
    priv_parallel_diff_msync();
#elif METALL_USE_PRIVATE_MAP_AND_PWRITE
    logger::out(logger::level::info, __FILE__, __LINE__, "pwrite() for the application data segment");
    priv_parallel_write_block();
#else
    logger::out(logger::level::info, __FILE__, __LINE__, "msync() for the application data segment");
    if (!util::os_msync(m_segment, m_current_segment_size, sync)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to msync the segment");
      return;
    }
#endif
    if (!util::mprotect_read_write(m_segment, m_current_segment_size)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to set the segment to readable and writable");
      return;
    }
  }

  bool priv_parallel_diff_msync() const {
    const auto num_threads = (int)std::min(m_block_fd_list.size(), (std::size_t)std::thread::hardware_concurrency());
    std::vector<std::thread *> threads(num_threads, nullptr);
    std::atomic_uint_fast64_t block_no_count = 0;

    auto diff_sync = [&block_no_count, this]() {
      while (true) {
        const auto block_no = block_no_count.fetch_add(1);
        if (block_no < m_block_fd_list.size()) {
          priv_diff_msync(block_no);
        } else {
          break;
        }
      }
    };

    logger::out(logger::level::info,
                __FILE__,
                __LINE__,
                "Sync files with " + std::to_string(num_threads) + " threads");
    for (int t = 0; t < num_threads; ++t) {
      threads[t] = new std::thread(diff_sync);
    }

    for (auto &th : threads) {
      th->join();
    }

    return true;
  }

  bool priv_diff_msync(const size_type block_no) const {
    const auto fd = m_block_fd_list[block_no];
    const auto private_map = static_cast<char *>(m_segment) + block_no * m_block_size;
    const auto
        shared_map = static_cast<char *>(util::map_file_write_mode(fd, nullptr, m_block_size, 0));
    if (!shared_map) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to map " + std::to_string(block_no) + "th file");
      return false;
    }

    // diff memcopy
    bool found_diff = false;
    for (size_type i = 0; i < m_block_size; ++i) {
      if (private_map[i] != shared_map[i]) {
        shared_map[i] = private_map[i];
        found_diff = true;
      }
    }

    if (found_diff) {
      if (!util::os_msync(shared_map, m_block_size, true)) {
        logger::out(logger::level::critical,
                    __FILE__,
                    __LINE__,
                    "Failed to msync " + std::to_string(block_no) + "th segment");
        return false;
      }
    }

    // Unmap shared map
    if (!util::os_munmap(shared_map, m_block_size)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to unmap " + std::to_string(block_no) + "th file");
      return false;
    }

    // Re-map the file to discard page cache
    if (!util::map_file_write_private_mode(fd, private_map, m_block_size, 0, MAP_FIXED)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to re-map " + std::to_string(block_no) + "th block with MAP_SHARED");
      return false;
    }

    return true;
  }

  bool priv_parallel_write_block() const {
    const auto num_threads = (int)std::min(m_block_fd_list.size(), (std::size_t)std::thread::hardware_concurrency());
    std::vector<std::thread *> threads(num_threads, nullptr);
    std::atomic_uint_fast64_t block_no_count = 0;

    auto write_block = [&block_no_count, this]() {
      while (true) {
        const auto block_no = block_no_count.fetch_add(1);
        if (block_no < m_block_fd_list.size()) {
          priv_write_block(block_no);
        } else {
          break;
        }
      }
    };

    logger::out(logger::level::info,
                __FILE__,
                __LINE__,
                "Write blocks with " + std::to_string(num_threads) + " threads");
    for (int t = 0; t < num_threads; ++t) {
      threads[t] = new std::thread(write_block);
    }

    for (auto &th : threads) {
      th->join();
    }

    return true;
  }

  bool priv_write_block(const size_type block_no) const {
    const auto fd = m_block_fd_list[block_no];
    const auto private_map = static_cast<char *>(m_segment) + block_no * m_block_size;

    constexpr ssize_t max_write_size = (1ULL << 30ULL);
    for (ssize_t total_written_size = 0; total_written_size < (ssize_t)m_block_size;
         total_written_size += max_write_size) {
      const auto write_size = std::min((ssize_t)max_write_size, (ssize_t)(m_block_size - total_written_size));
      const auto written_size = ::pwrite(fd, private_map, write_size, total_written_size);
      if (written_size == -1) {
        logger::perror(logger::level::error, __FILE__, __LINE__, "pwrite");
        return false;
      }

      if ((ssize_t)written_size != (ssize_t)write_size) {
        logger::out(logger::level::critical,
                    __FILE__,
                    __LINE__,
                    std::to_string(written_size) + " bytes (" + std::to_string(write_size)
                        + " bytes is expected) is written at " + std::to_string(block_no) + "th block.");
        return false;
      }
    }

    // Re-map the file to discard page cache
    if (!util::map_file_write_private_mode(fd, private_map, m_block_size, 0, MAP_FIXED)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to re-map " + std::to_string(block_no) + "th block with MAP_SHARED");
      return false;
    }

    return true;
  }

  bool priv_free_region(const different_type offset, const size_type nbytes) const {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

    if (m_free_file_space)
      return priv_free_file_backed_region(offset, nbytes);
    else
      return priv_uncommit_pages(offset, nbytes);
  }

  bool priv_free_file_backed_region(const different_type offset, const size_type nbytes) const {
#if (METALL_USE_PRIVATE_MAP_AND_MSYNC || METALL_USE_PRIVATE_MAP_AND_PWRITE)
    // Uncommit pages in DRAM first
    if (!util::uncommit_private_pages(static_cast<char *>(m_segment) + offset, nbytes)) return false;

    // Free space in file
    auto sub_offset = offset;
    auto remain_nbyte = nbytes;
    auto block_no = offset / m_block_size;
    while (remain_nbyte > 0) {
      const auto offset_within_block = sub_offset - block_no * m_block_size;
      const auto nbyte_to_free = std::min(m_block_size - offset_within_block, remain_nbyte);
      if (!util::free_file_space(m_block_fd_list[block_no], offset_within_block, nbyte_to_free)) return false;

      remain_nbyte -= nbyte_to_free;
      ++block_no;
      sub_offset = block_no * m_block_size;
    }

    return true;
#else
    return util::uncommit_file_backed_pages(static_cast<char *>(m_segment) + offset, nbytes);
#endif
  }

  bool priv_uncommit_pages(const different_type offset, const size_type nbytes) const {
#if (METALL_USE_PRIVATE_MAP_AND_MSYNC || METALL_USE_PRIVATE_MAP_AND_PWRITE)
    return util::uncommit_private_pages(static_cast<char *>(m_segment) + offset, nbytes);
#else
    return util::uncommit_shared_pages(static_cast<char *>(m_segment) + offset, nbytes);
#endif
  }

  bool priv_load_system_page_size() {
    m_system_page_size = util::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  void priv_test_file_space_free(const std::string &base_path) {
#ifdef DISABLE_FREE_FILE_SPACE
    m_free_file_space = false;
    return;
#endif

    assert(m_system_page_size > 0);
    const std::string file_path(base_path + "_test");
    const size_type file_size = m_system_page_size * 2;

    if (!util::create_file(file_path)) return;
    if (!util::extend_file_size(file_path, file_size)) return;
    assert(static_cast<size_type>(util::get_file_size(file_path)) >= file_size);

    const auto ret = util::map_file_write_mode(file_path, nullptr, file_size, 0);
    if (ret.first == -1 || !ret.second) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to map file: " + file_path);
      if (ret.first == -1) util::os_close(ret.first);
      return;
    }

    // Test freeing file space
    char *buf = static_cast<char *>(ret.second);
    buf[0] = 0;
#if (METALL_USE_PRIVATE_MAP_AND_MSYNC || METALL_USE_PRIVATE_MAP_AND_PWRITE)
    if (util::uncommit_private_pages(ret.second, file_size) && util::free_file_space(ret.first, 0, file_size)) {
#else
      if (util::uncommit_file_backed_pages(ret.second, file_size)) {
#endif
      m_free_file_space = true;
    } else {
      m_free_file_space = false;
    }

    if (!util::os_close(ret.first)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to close file: " + file_path);
      return;
    }

    // Closing
    util::munmap(ret.second, file_size, false);
    if (!util::remove_file(file_path)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to remove a file: " + file_path);
      return;
    }

    logger::out(logger::level::info,
                __FILE__,
                __LINE__,
                std::string("File free test result: ") + (m_free_file_space ? "success" : "failed"));
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  ssize_t m_system_page_size{0};
  size_type m_num_blocks{0};
  size_type m_vm_region_size{0};
  size_type m_current_segment_size{0};
  void *m_segment{nullptr};
  std::string m_base_path;
  bool m_read_only{false};
  bool m_free_file_space{true};
  std::vector<int> m_block_fd_list;
  size_type m_block_size{0};
};

} // namespace kernel
} // namespace metall
#endif //METALL_KERNEL_SEGMENT_STORAGE_MMAP_SEGMENT_STORAGE_HPP

