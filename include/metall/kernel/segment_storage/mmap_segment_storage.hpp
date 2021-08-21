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
#include <memory>

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
  mmap_segment_storage() {
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    // TODO: implement msync for anonymous mapping
    static_assert(true, "METALL_USE_ANONYMOUS_NEW_MAP does not work now");
    logger::out(logger::level::info, __FILE__, __LINE__, "METALL_USE_ANONYMOUS_NEW_MAP is defined");
#endif

    if (!priv_set_system_page_size()) {
      priv_set_broken_status();
    }
  }

  ~mmap_segment_storage() {
    if (!sync(true) || !destroy()) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to destruct");
    }
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
    other.priv_set_broken_status();
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

    other.priv_set_broken_status();
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
    return priv_copy(source_path, destination_path, clone, max_num_threads);
  }

  /// \brief Creates a new segment.
  /// Calling this function fails if this class already manages an opened segment.
  /// \base_path A base directory path to create a segment.
  /// \param vm_region_size The size of a reserved VM region.
  /// \param vm_region The address of a reserved VM region.
  /// \block_size The block size.
  /// \return Return true if success; otherwise, false.
  bool create(const std::string &base_path,
              const size_type vm_region_size,
              void *const vm_region,
              const size_type block_size) {
    return priv_create(base_path, vm_region_size, vm_region, block_size);
  }

  /// \brief Opens an existing segment.
  /// Calling this function fails if this class already manages an opened segment.
  /// \base_path A base directory path to create a segment.
  /// \param vm_region_size The size of a VM region.
  /// \param vm_region The address of a VM region.
  /// \param read_only If true, this segment is read only.
  /// \return Return true if success; otherwise, false.
  bool open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    return priv_open(base_path, vm_region_size, vm_region, read_only);
  }

  /// \brief Extends the currently opened segment if necessary.
  /// \param request_size A segment size to extend to.
  /// \return Returns true if the segment is extended to or already larger than the requested size.
  /// Returns false on failure.
  bool extend(const size_type request_size) {
    return priv_extend(request_size);
  }

  /// \brief Destroys the segment --- the data will be lost.
  /// To save data to files, sync() must be called beforehand.
  bool destroy() {
    return priv_destroy_segment();
  }

  /// \brief Syncs the segment with backing files.
  /// \param sync If false is specified, this function returns before finishing the sync operation.
  bool sync(const bool sync) {
    if (!priv_sync_segment(sync)) { // Failing this operation is not a critical error
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to synchronize the segment");
      return false;
    }
    return true;
  }

  /// \brief Tries to free the specified region in DRAM and file(s).
  /// The actual behavior depends on the system running on.
  /// \param offset An offset to the region from the beginning of the segment.
  /// \param nbytes The size of the region.
  bool free_region(const different_type offset, const size_type nbytes) {
    return priv_free_region(offset, nbytes); // Failing this operation is not a critical error
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

  /// \brief Checks if there is a segment already open.
  /// \return Returns true if there is a segment already open.
  bool is_open() const {
    return priv_is_open();
  }

  /// \brief Checks the sanity.
  /// \return Returns true if there is no issue; otherwise, returns false.
  /// If false is returned, the instance of this class cannot be used anymore.
  bool check_sanity() const {
    return !m_broken;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  static std::string priv_make_block_file_name(const std::string &base_path, const size_type n) {
    return base_path + "/block-" + std::to_string(n);
  }

  void priv_set_broken_status() {
    m_system_page_size = 0;
    m_num_blocks = 0;
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_segment = nullptr;
    m_broken = true;
    // m_read_only must not be modified here.
  }

  bool priv_is_open() const {
    return (check_sanity() && m_system_page_size > 0 && m_num_blocks > 0 && m_vm_region_size > 0 && m_current_segment_size > 0
        && m_segment && !m_base_path.empty());
  }

  static bool priv_copy(const std::string &source_path,
                        const std::string &destination_path,
                        const bool clone,
                        const int max_num_threads) {
    if (!mdtl::directory_exist(destination_path)) {
      if (!mdtl::create_directory(destination_path)) {
        std::string s("Cannot create a directory: " + destination_path);
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
        return false;
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

  bool priv_create(const std::string &base_path,
                   const size_type vm_region_size,
                   void *const vm_region,
                   const size_type block_size) {
    if (!check_sanity()) return false;
    if (is_open()) return false; // Cannot overwrite an existing segment.

    {
      std::string s("Create a segment under: " + base_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }

    if (!mdtl::directory_exist(base_path)) {
      if (!mdtl::create_directory(base_path)) {
        std::string s("Cannot create a directory: " + base_path);
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
        // As no internal value has been changed, m_broken is still false.
        return false;
      }
    }

    m_block_size = mdtl::round_up(std::min(vm_region_size, block_size), page_size());
    m_base_path = base_path;
    m_vm_region_size = mdtl::round_down(vm_region_size, page_size());
    m_segment = mdtl::round_up(reinterpret_cast<intptr_t>(vm_region), page_size()) + reinterpret_cast<char *>(0);
    m_read_only = false;

    if (!priv_create_new_map(m_base_path, 0, m_block_size, 0)) {
      priv_set_broken_status();
      return false;
    }
    m_current_segment_size = m_block_size;
    m_num_blocks = 1;

    if (!priv_test_file_space_free(base_path)) {
      std::string s("Failed to test file space free: " + base_path);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_destroy_segment();
      priv_set_broken_status();
      return false;
    }

    return true;
  }

  bool priv_open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    if (!check_sanity()) return false;
    if (is_open()) return false; // Cannot overwrite an existing segment.

    std::string s("Open a segment under: " + base_path);
    logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());

    m_base_path = base_path;
    m_vm_region_size = mdtl::round_down(vm_region_size, page_size());;
    m_segment = mdtl::round_up(reinterpret_cast<intptr_t>(vm_region), page_size()) + reinterpret_cast<char *>(0);
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
        logger::out(logger::level::error, __FILE__, __LINE__, "File sizes are not the same");
        priv_destroy_segment();
        priv_set_broken_status();
        return false;
      }
      m_block_size = file_size;

      const auto fd = priv_map_file(file_name, m_block_size, m_current_segment_size, read_only);
      if (fd == -1) {
        std::stringstream ss;
        ss << "Failed to map a file " << m_block_size;
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
        priv_destroy_segment();
        priv_set_broken_status();
        return false;
      } else {
        m_block_fd_list.template emplace_back(fd);
      }
      m_current_segment_size += m_block_size;
      ++m_num_blocks;
    }

    if (!read_only && !priv_test_file_space_free(base_path)) {
      std::string s("Failed to test file space free: " + base_path);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_destroy_segment();
      priv_set_broken_status();
      return false;
    }

    if (m_num_blocks == 0) {
      priv_set_broken_status();
      return false;
    }

    return true;
  }

  bool priv_extend(const size_type request_size) {
    if (!is_open()) return false;

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
      if (!priv_create_new_map(m_base_path,
                               m_num_blocks,
                               m_block_size,
                               m_current_segment_size)) {
        logger::out(logger::level::error, __FILE__, __LINE__, "Failed to extend the segment");
        priv_destroy_segment();
        priv_set_broken_status();
        return false;
      }
      ++m_num_blocks;
      m_current_segment_size += m_block_size;
    }

    return true;
  }

  bool priv_create_new_map(const std::string &base_path,
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
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    if (!priv_map_anonymous(file_name, file_size, segment_offset)) {
      return false;
    }
    // Although we do not map the file, we still open it so that other functions in this class work.
    const auto fd = ::open(file_name.c_str(), O_RDWR);
    if (fd == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "open");
      std::string s("Failed to open a file " + file_name);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      // Destroy the map by overwriting PROT_NONE map since the VM region is managed by another class.
      mdtl::map_with_prot_none(static_cast<char *>(m_segment) + segment_offset, file_size);
      return false;
    }
    m_block_fd_list.emplace_back(fd);
#else
    const auto fd = priv_map_file(file_name, file_size, segment_offset, false);
    if (fd == -1) {
      return false;
    }
    m_block_fd_list.emplace_back(fd);
#endif

    return true;
  }

  int priv_map_file(const std::string &path,
                    const size_type file_size,
                    const different_type segment_offset,
                    const bool read_only) const {
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

    {
      std::stringstream ss;
      ss << "Map a file " << path << " at " << segment_offset <<
         " with " << file_size << " bytes; read-only mode is " << std::to_string(read_only);
      logger::out(logger::level::info, __FILE__, __LINE__, ss.str().c_str());
    }

    const auto ret = (read_only) ?
                     mdtl::map_file_read_mode(path, map_addr, file_size, 0, MAP_FIXED) :
                     mdtl::map_file_write_mode(path, map_addr, file_size, 0, MAP_FIXED | map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map a file: " + path);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) {
        mdtl::os_close(ret.first);
      }
      return -1;
    }

    return ret.first;
  }

  bool priv_map_anonymous(const std::string &path,
                          const size_type region_size,
                          const different_type segment_offset) const {
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
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    return true;
  }

  bool priv_destroy_segment() {
    if (!is_open()) return false;

    int succeeded = true;
    for (const auto &fd : m_block_fd_list) {
      succeeded &= mdtl::os_close(fd);
    }

    // Destroy the mapping region by calling mmap with PROT_NONE over the region.
    // As the unmap system call syncs the data first, this approach is significantly fast.
    succeeded &= mdtl::map_with_prot_none(m_segment, m_current_segment_size);
    // NOTE: the VM region will be unmapped by another class

    priv_set_broken_status();

    return succeeded;
  }

  bool priv_sync_segment(const bool sync) const {
    if (!is_open()) return false;

    if (m_read_only) return true;

    // Protect the region to detect unexpected write by application during msync
    if (!mdtl::mprotect_read_only(m_segment, m_current_segment_size)) {
      logger::out(logger::level::error,
                  __FILE__,
                  __LINE__,
                  "Failed to protect the segment with the read only mode");
      return false;
    }

    logger::out(logger::level::info, __FILE__, __LINE__, "msync() for the application data segment");
    if (!priv_parallel_msync(sync)) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to msync the segment");
      return false;
    }
    if (!mdtl::mprotect_read_write(m_segment, m_current_segment_size)) {
      logger::out(logger::level::error,
                  __FILE__,
                  __LINE__,
                  "Failed to set the segment to readable and writable");
      return false;
    }

    return true;
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
    std::vector<std::unique_ptr<std::thread>> threads(num_threads);
    for (auto &th : threads) {
      th =  std::make_unique<std::thread>(diff_sync);
    }

    for (auto &th : threads) {
      th->join();
    }

    return num_successes == m_block_fd_list.size();
  }

  bool priv_free_region(const different_type offset, const size_type nbytes) const {
    if (!is_open() || m_read_only) return false;

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

  bool priv_set_system_page_size() {
    m_system_page_size = mdtl::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  bool priv_test_file_space_free(const std::string &base_path) {
#ifdef METALL_DISABLE_FREE_FILE_SPACE
    m_free_file_space = false;
    return true;
#endif

    assert(m_system_page_size > 0);
    const std::string file_path(base_path + "/test");
    const size_type file_size = m_system_page_size * 2;

    if (!mdtl::create_file(file_path)) return false;
    if (!mdtl::extend_file_size(file_path, file_size)) return false;
    assert(static_cast<size_type>(mdtl::get_file_size(file_path)) >= file_size);

    const auto ret = mdtl::map_file_write_mode(file_path, nullptr, file_size, 0);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map file: " + file_path);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) mdtl::os_close(ret.first);
      return false;
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
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    // Closing
    mdtl::munmap(ret.second, file_size, false);
    if (!mdtl::remove_file(file_path)) {
      std::string s("Failed to remove a file: " + file_path);
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    {
      std::string s("File free test result: ");
      s += m_free_file_space ? "success" : "failed";
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    }

    return true;
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
  bool m_broken{false};
};

} // namespace kernel
} // namespace metall
#endif //METALL_KERNEL_SEGMENT_STORAGE_MMAP_SEGMENT_STORAGE_HPP

