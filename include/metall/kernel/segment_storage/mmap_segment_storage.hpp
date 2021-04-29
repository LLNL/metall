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

#include <metall/detail/file.hpp>
#include <metall/detail/file_clone.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
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
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    logger::out(logger::level::info, __FILE__, __LINE__, "METALL_USE_ANONYMOUS_NEW_MAP is defined");
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

  /// \brief Gets the size of an existing segment.
  /// This is a static version of size() method.
  /// \param base_path A path to a segment.
  static size_type get_size(const std::string &base_path) {
    int block_no = 0;
    size_type total_file_size = 0;
    while (true) {
      const auto file_name = priv_make_block_file_name(base_path, block_no);
      if (!mdtl::file_exist(file_name)) {
        break;
      }
      total_file_size += mdtl::get_file_size(file_name);
      ++block_no;
    }
    return total_file_size;
  }

  /// \brief Checks if a segment is openable.
  /// \param base_path A path to a segment.
  /// \return Return true if success; otherwise, false.
  static bool openable(const std::string &base_path) {
    const auto file_name = priv_make_block_file_name(base_path, 0);
    return mdtl::file_exist(file_name);
  }

  /// \brief Copies segment to another location.
  /// \param source_path A path to a source segment.
  /// \param destination_path A destination path.
  /// \param clone If true, uses clone (reflink) for copying files.
  /// \param max_num_threads The maximum number of threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Return true if success; otherwise, false.
  static bool copy(const std::string &source_path,
                   const std::string &destination_path,
                   const bool clone,
                   const int max_num_threads) {
    if (!mdtl::directory_exist(destination_path)) {
      if (!mdtl::create_directory(destination_path)) {
        std::string s("Cannot create a directory: " + destination_path);
        logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      }
    }

    if (clone) {
      std::string s("Clone: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::clone_files_in_directory_in_parallel(source_path, destination_path, max_num_threads);
    } else {
      std::string s("Copy: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::copy_files_in_directory_in_parallel(source_path, destination_path, max_num_threads);
    }
    assert(false);
    return false;
  }

  /// \brief Creates a new segment.
  /// \base_path A base directory path to create a segment.
  /// \param vm_region_size The size of a VM region.
  /// \param vm_region The address of a VM region.
  /// \block_size The block size.
  /// \return Return true if success; otherwise, false.
  bool create(const std::string &base_path,
              const size_type vm_region_size,
              void *const vm_region,
              const size_type block_size) {
    assert(!priv_inited());
    m_block_size = std::min(vm_region_size, block_size);

    {
      std::string s("Create a segment under: " + base_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }

    if (!mdtl::directory_exist(base_path)) {
      if (!mdtl::create_directory(base_path)) {
        std::string s("Cannot create a directory: " + base_path);
        logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      }
    }

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

  /// \brief Opens an existing segment.
  /// \base_path A base directory path to create a segment.
  /// \param vm_region_size The size of a VM region.
  /// \param vm_region The address of a VM region.
  /// \param read_only If true, this segment is read only.
  /// \return Return true if success; otherwise, false.
  bool open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    assert(!priv_inited());

    std::string s("Open a segment under: " + base_path);
    logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());

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
      const auto file_name = priv_make_block_file_name(m_base_path, m_num_blocks);
      if (!mdtl::file_exist(file_name)) {
        break;
      }

      const auto file_size = mdtl::get_file_size(file_name);
      assert(file_size % page_size() == 0);
      if (m_block_size > 0 && m_block_size != (size_type)file_size) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "File sizes are not the same");
        return false;
      }
      m_block_size = file_size;

      if (!priv_map_file(file_name, m_block_size, m_current_segment_size, read_only)) {
        std::stringstream ss;
        ss << "Failed to map a file " << m_block_size;
        logger::out(logger::level::critical, __FILE__, __LINE__, ss.str().c_str());
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

  /// \brief Extends the currently opened segment if necessary.
  /// \param request_size A segment size to extend to.
  /// \return Returns true if the segment is extended to or already larger than the requested size.
  /// Returns false on failure.
  bool extend(const size_type request_size) {
    assert(priv_inited());

    if (m_read_only) {
      return false;
    }

    if (request_size > m_vm_region_size) {
      logger::out(logger::level::error,
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
  void sync(const bool sync) {
    priv_sync_segment(sync);
  }

  /// \brief Tries to free the specified region in DRAM and file(s).
  /// The actual behavior depends on the system running on.
  /// \param offset An offset to the region from the beginning of the segment.
  /// \param nbytes The size of the region.
  void free_region(const different_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  /// \brief Returns the address of the segment.
  /// \return The address of the segment.
  void *get_segment() const {
    return m_segment;
  }

  /// \brief Returns the current size.
  /// \return The current segment size.
  size_type size() const {
    return m_current_segment_size;
  }

  /// \brief Returns the page size.
  /// \return The page size of the system.
  size_type page_size() const {
    return m_system_page_size;
  }

  /// \brief Checks if the segment is read only.
  /// \return Returns true if the segment is read only; otherwise, returns false.
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
  static std::string priv_make_block_file_name(const std::string &base_path, const size_type n) {
    return base_path + "/block-" + std::to_string(n);
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
    const std::string file_name = priv_make_block_file_name(base_path, block_number);
    {
      std::string s("Create and extend a file " + file_name + " with " + std::to_string(file_size) + " bytes");
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }

    if (!mdtl::create_file(file_name)) return false;
    if (!mdtl::extend_file_size(file_name, file_size)) return false;
    if (static_cast<size_type>(mdtl::get_file_size(file_name)) < file_size) {
      std::string s("Failed to create and extend file: " + file_name);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      return false;
    }

#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    if (!priv_map_anonymous(file_name, file_size, segment_offset)) {
      return false;
    }
#else
    if (!priv_map_file(file_name, file_size, segment_offset, false)) {
      return false;
    }
#endif

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
    std::stringstream ss;
    ss << "Map a file " << path << " at " << segment_offset <<
       " with " << file_size << " bytes; read-only mode is " << std::to_string(read_only);
    logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());

    const auto ret = (read_only) ?
                     mdtl::map_file_read_mode(path, map_addr, file_size, 0, MAP_FIXED) :
                     mdtl::map_file_write_mode(path, map_addr, file_size, 0, MAP_FIXED | map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map a file: " + path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) {
        mdtl::os_close(ret.first);
      }
      return false;
    }

    m_block_fd_list.emplace_back(ret.first);

    return true;
  }

  bool priv_map_anonymous(const std::string &path,
                          const size_type region_size,
                          const different_type segment_offset) {
    assert(!path.empty());
    assert(region_size > 0);
    assert(segment_offset >= 0);
    assert(segment_offset + region_size <= m_vm_region_size);

    const auto map_addr = static_cast<char *>(m_segment) + segment_offset;
    {
      std::string s("Map an anonymous region at " + std::to_string(segment_offset) + " with "
                        + std::to_string(region_size));
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }

    const auto *addr = mdtl::map_anonymous_write_mode(map_addr, region_size, MAP_FIXED);
    if (!addr) {
      std::string s("Failed to map an anonymous region at " + std::to_string(segment_offset));
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      return false;
    }

    // Although we do not map the file, we still open it so that other functions in this class work.
    const int fd = ::open(path.c_str(), O_RDWR);
    if (fd == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "open");
      std::string s("Failed to open a file " + path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
    }

    m_block_fd_list.emplace_back(fd);

    return true;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    for (const auto &fd : m_block_fd_list) {
      mdtl::os_close(fd);
    }

    // Destroy the mapping region by calling mmap with PROT_NONE over the region.
    // As the unmap system call syncs the data first, this approach is significantly fast.
    mdtl::map_with_prot_none(m_segment, m_current_segment_size);
    // NOTE: the VM region will be unmapped by manager_kernel

    priv_reset();
  }

  void priv_sync_segment(const bool sync) {
    if (!priv_inited() || m_read_only) return;

    // Protect the region to detect unexpected write by application during msync
    if (!mdtl::mprotect_read_only(m_segment, m_current_segment_size)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to protect the segment with the read only mode");
      return;
    }

    logger::out(logger::level::info, __FILE__, __LINE__, "msync() for the application data segment");
    if (!priv_parallel_msync(sync)) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to msync the segment");
      return;
    }
    if (!mdtl::mprotect_read_write(m_segment, m_current_segment_size)) {
      logger::out(logger::level::critical,
                  __FILE__,
                  __LINE__,
                  "Failed to set the segment to readable and writable");
      return;
    }
  }

  bool priv_parallel_msync(const bool sync) const {

    std::atomic_uint_fast64_t block_no_count = 0;
    std::atomic_uint_fast64_t num_successes = 0;
    auto diff_sync = [&sync, &block_no_count, &num_successes, this]() {
      while (true) {
        const auto block_no = block_no_count.fetch_add(1);
        if (block_no < m_block_fd_list.size()) {
          const auto map = static_cast<char *>(m_segment) + block_no * m_block_size;
          num_successes.fetch_add(mdtl::os_msync(map, m_block_size, sync) ? 1 : 0);
        } else {
          break;
        }
      }
    };

    const auto num_threads = (int)std::min(m_block_fd_list.size(), (std::size_t)std::thread::hardware_concurrency());
    {
      std::stringstream ss;
      ss << "Sync files with " << num_threads << " threads";
      logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());
    }
    std::vector<std::thread *> threads(num_threads, nullptr);
    for (auto &th : threads) {
      th = new std::thread(diff_sync);
    }

    for (auto &th : threads) {
      th->join();
    }

    return num_successes == m_block_fd_list.size();
  }

  bool priv_free_region(const different_type offset, const size_type nbytes) const {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

    if (m_free_file_space)
      return priv_uncommit_pages_and_free_file_space(offset, nbytes);
    else
      return priv_uncommit_pages(offset, nbytes);
  }

  bool priv_uncommit_pages_and_free_file_space(const different_type offset, const size_type nbytes) const {
    return mdtl::uncommit_shared_pages_and_free_file_space(static_cast<char *>(m_segment) + offset, nbytes);
  }

  bool priv_uncommit_pages(const different_type offset, const size_type nbytes) const {
    return mdtl::uncommit_shared_pages(static_cast<char *>(m_segment) + offset, nbytes);
  }

  bool priv_load_system_page_size() {
    m_system_page_size = mdtl::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  void priv_test_file_space_free(const std::string &base_path) {
#ifdef METALL_DISABLE_FREE_FILE_SPACE
    m_free_file_space = false;
    return;
#endif

    assert(m_system_page_size > 0);
    const std::string file_path(base_path + "/test");
    const size_type file_size = m_system_page_size * 2;

    if (!mdtl::create_file(file_path)) return;
    if (!mdtl::extend_file_size(file_path, file_size)) return;
    assert(static_cast<size_type>(mdtl::get_file_size(file_path)) >= file_size);

    const auto ret = mdtl::map_file_write_mode(file_path, nullptr, file_size, 0);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map file: " + file_path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) mdtl::os_close(ret.first);
      return;
    }

    // Test freeing file space
    char *buf = static_cast<char *>(ret.second);
    buf[0] = 0;
    if (mdtl::uncommit_shared_pages_and_free_file_space(ret.second, file_size)) {
      m_free_file_space = true;
    } else {
      m_free_file_space = false;
    }

    if (!mdtl::os_close(ret.first)) {
      std::string s("Failed to close file: " + file_path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      return;
    }

    // Closing
    mdtl::munmap(ret.second, file_size, false);
    if (!mdtl::remove_file(file_path)) {
      std::string s("Failed to remove a file: " + file_path);
      logger::out(logger::level::critical, __FILE__, __LINE__, s.c_str());
      return;
    }

    {
      std::string s("File free test result: ");
      s += m_free_file_space ? "success" : "failed";
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }
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

