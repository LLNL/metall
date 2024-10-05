// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_SEGMENT_STORAGE_HPP
#define METALL_KERNEL_SEGMENT_STORAGE_HPP

#include <string>
#include <iostream>
#include <cassert>
#include <thread>
#include <atomic>
#include <memory>

#include "metall/defs.hpp"
#include "metall/detail/file.hpp"
#include "metall/detail/file_clone.hpp"
#include "metall/detail/mmap.hpp"
#include "metall/detail/utilities.hpp"
#include "metall/logger.hpp"
#include "metall/kernel/storage.hpp"
#include "metall/kernel/segment_header.hpp"

namespace metall::kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

class segment_storage {
 private:
  static constexpr const char *k_dir_name = "segment";

#ifndef METALL_SEGMENT_BLOCK_SIZE
#error "METALL_SEGMENT_BLOCK_SIZE is not defined."
#endif
  // TODO: check block size is a multiple of page size
  static constexpr std::size_t k_block_size = METALL_SEGMENT_BLOCK_SIZE;

 public:
  using path_type = storage::path_type;
  using segment_header_type = segment_header;

  segment_storage() {
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    logger::out(logger::level::verbose, __FILE__, __LINE__,
                "METALL_USE_ANONYMOUS_NEW_MAP is defined");
#endif

    if (!priv_set_system_page_size()) {
      priv_set_broken_status();
    }
  }

  ~segment_storage() {
    int ret = true;
    if (is_open()) {
      ret &= sync(true);
      if (!ret) {
        ret &= release();
      }
    }

    if (!ret) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to destruct");
    }
  }

  segment_storage(const segment_storage &) = delete;
  segment_storage &operator=(const segment_storage &) = delete;

  segment_storage(segment_storage &&other) noexcept
      : m_system_page_size(other.m_system_page_size),
        m_num_blocks(other.m_num_blocks),
        m_vm_region_size(other.m_vm_region_size),
        m_segment_capacity(other.m_segment_capacity),
        m_current_segment_size(other.m_current_segment_size),
        m_vm_region(other.m_vm_region),
        m_segment(other.m_segment),
        m_segment_header(other.m_segment_header),
        m_top_path(other.m_top_path),
        m_read_only(other.m_read_only),
        m_free_file_space(other.m_free_file_space),
        m_block_fd_list(std::move(other.m_block_fd_list))
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
        ,
        m_anonymous_map_flag_list(other.m_anonymous_map_flag_list)
#endif
  {
    other.priv_set_broken_status();
  }

  segment_storage &operator=(segment_storage &&other) noexcept {
    m_system_page_size = other.m_system_page_size;
    m_num_blocks = other.m_num_blocks;
    m_vm_region_size = other.m_vm_region_size;
    m_segment_capacity = other.m_segment_capacity;
    m_current_segment_size = other.m_current_segment_size;
    m_vm_region = other.m_vm_region;
    m_segment_header = other.m_segment_header;
    m_segment = other.m_segment;
    m_top_path = std::move(other.m_top_path);
    m_read_only = other.m_read_only;
    m_free_file_space = other.m_free_file_space;
    m_block_fd_list = std::move(other.m_block_fd_list);
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    m_anonymous_map_flag_list = std::move(other.m_anonymous_map_flag_list);
#endif
    other.priv_set_broken_status();
    return (*this);
  }

  /// \brief Copies segment to another location.
  /// \param source_path A path to a source segment.
  /// \param destination_path A destination path.
  /// \param clone If true, uses clone (reflink) for copying files.
  /// \param max_num_threads The maximum number of threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Return true if success; otherwise, false.
  static bool copy(const path_type &source_path,
                   const path_type &destination_path, const bool clone,
                   const int max_num_threads) {
    return priv_copy(priv_top_dir_path(source_path),
                     priv_top_dir_path(destination_path), clone,
                     max_num_threads);
  }

  /// \brief Creates a new segment.
  /// Calling this function fails if this class already manages an opened
  /// segment.
  /// \base_path A base directory path to create a segment.
  /// \param capacity A segment capacity to reserve.
  /// Return true if success; otherwise, false.
  bool create(const path_type &base_path, const std::size_t capacity) {
    return priv_create(priv_top_dir_path(base_path), capacity);
  }

  /// \brief Opens an existing segment.
  /// Calling this function fails if this class already manages an opened
  /// segment.
  /// \param base_path A base directory path to open a segment.
  /// \param capacity A segment capacity to reserve.
  /// This value will is ignored if read_only is true.
  /// \param read_only If true, this segment is read only.
  /// \return Return true if success; otherwise, false.
  bool open(const path_type &base_path, const std::size_t capacity,
            const bool read_only) {
    return priv_open(priv_top_dir_path(base_path), capacity, read_only);
  }

  /// \brief Extends the currently opened segment if necessary.
  /// \param request_size A segment size to extend to.
  /// \return Returns true if the segment is extended to or already larger than
  /// the requested size. Returns false on failure.
  bool extend(const std::size_t request_size) {
    return priv_extend(request_size);
  }

  /// \brief Releases the segment --- the data will be lost.
  /// To save data to files, sync() must be called beforehand.
  bool release() { return priv_release_segment(); }

  /// \brief Syncs the segment with backing files.
  /// \param sync If false is specified, this function returns before finishing
  /// the sync operation.
  bool sync(const bool sync) { return priv_sync(sync); }

  /// \brief Tries to free the specified region in DRAM and file(s).
  /// The actual behavior depends on the running system.
  /// \param offset An offset to the region from the beginning of the segment.
  /// \param nbytes The size of the region.
  bool free_region(const std::ptrdiff_t offset, const std::size_t nbytes) {
    return priv_free_region(
        offset, nbytes);  // Failing this operation is not a critical error
  }

  /// \brief Takes a snapshot of the segment.
  /// \param snapshot_path A path to a snapshot.
  /// \param clone If true, uses clone (reflink) for copying files.
  /// \param max_num_threads The maximum number of threads to use.
  /// If <= 0 is given, the value is automatically determined.
  /// \return Return true if success; otherwise, false.
  bool snapshot(const path_type &snapshot_path, const bool clone,
                const int max_num_threads) {
    sync(true);
    return priv_copy(m_top_path, priv_top_dir_path(snapshot_path), clone,
                     max_num_threads);
  }

  /// \brief Returns the address of the segment.
  /// \return The address of the segment.
  void *get_segment() const { return m_segment; }

  /// \brief Returns a reference to the segment header.
  /// \return A reference to the segment header.
  segment_header_type &get_segment_header() {
    return *reinterpret_cast<segment_header_type *>(m_vm_region);
  }

  /// \brief Returns a reference to the segment header.
  /// \return A reference to the segment header.
  const segment_header_type &get_segment_header() const {
    return *reinterpret_cast<const segment_header_type *>(m_vm_region);
  }

  /// \brief Returns the current segment size.
  /// \return The current segment size.
  std::size_t size() const { return m_current_segment_size; }

  /// \brief Returns the underlying page size.
  /// \return The page size of the system.
  std::size_t page_size() const { return m_system_page_size; }

  /// \brief Checks if the segment is read only.
  /// \return Returns true if the segment is read only; otherwise, returns
  /// false.
  bool read_only() const { return m_read_only; }

  /// \brief Checks if there is a segment already open.
  /// \return Returns true if there is a segment already open.
  bool is_open() const { return priv_is_open(); }

  /// \brief Checks the sanity of the instance.
  /// \return Returns true if there is no issue; otherwise, returns false.
  /// If false is returned, the instance of this class cannot be used anymore.
  bool check_sanity() const { return !m_broken; }

 private:
  static path_type priv_top_dir_path(const path_type &base_path) {
    return storage::get_path(base_path, k_dir_name);
  }

  /// \warning This function takes 'top_path' as an argument instead of
  /// 'base_path'.
  static path_type priv_block_file_path(const path_type &top_path,
                                        const std::size_t n) {
    return top_path / ("block-" + std::to_string(n));
  }

  static bool priv_openable(const path_type &top_path) {
    const auto file_name = priv_block_file_path(top_path, 0);
    return mdtl::file_exist(file_name);
  }

  static std::size_t priv_get_size(const path_type &top_path) {
    int block_no = 0;
    std::size_t total_file_size = 0;
    while (true) {
      const auto file_name = priv_block_file_path(top_path, block_no);
      if (!mdtl::file_exist(file_name)) {
        break;
      }
      total_file_size += mdtl::get_file_size(file_name);
      ++block_no;
    }
    return total_file_size;
  }

  std::size_t priv_round_up_to_block_size(const std::size_t nbytes) const {
    const auto alignment =
        std::max((size_t)m_system_page_size, (size_t)k_block_size);
    return mdtl::round_up(nbytes, alignment);
  }

  std::size_t priv_round_down_to_block_size(const std::size_t nbytes) const {
    const auto alignment =
        std::max((size_t)m_system_page_size, (size_t)k_block_size);
    return mdtl::round_down(nbytes, alignment);
  }

  void priv_clear_status() {
    m_system_page_size = 0;
    m_num_blocks = 0;
    m_vm_region_size = 0;
    m_segment_capacity = 0;
    m_current_segment_size = 0;
    m_vm_region = nullptr;
    m_segment = nullptr;
    m_segment_header = nullptr;
    // m_read_only must not be modified here.
  }

  void priv_set_broken_status() {
    priv_clear_status();
    m_broken = true;
  }

  bool priv_is_open() const {
    // TODO: brush up this logic
    return (check_sanity() && m_system_page_size > 0 && m_num_blocks > 0 &&
            m_vm_region_size > 0 && m_segment_capacity > 0 &&
            m_current_segment_size > 0 && m_vm_region && m_segment &&
            !m_top_path.empty() && !m_block_fd_list.empty());
  }

  static bool priv_copy(const path_type &source_path,
                        const path_type &destination_path, const bool clone,
                        const int max_num_threads) {
    if (!mdtl::directory_exist(destination_path)) {
      if (!mdtl::create_directory(destination_path)) {
        std::string s("Cannot create a directory: " +
                      destination_path.string());
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
        return false;
      }
    }

    if (clone) {
      std::string s("Clone: " + source_path.string());
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
      return mdtl::clone_files_in_directory_in_parallel(
          source_path, destination_path, max_num_threads);
    } else {
      std::string s("Copy: " + source_path.string());
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
      return mdtl::copy_files_in_directory_in_parallel(
          source_path, destination_path, max_num_threads);
    }
    assert(false);
    return false;
  }

  bool priv_prepare_header_and_segment(
      const std::size_t segment_capacity_request) {
    const auto header_size = mdtl::round_up(sizeof(segment_header_type),
                                            int64_t(m_system_page_size));
    const auto vm_region_size =
        header_size + priv_round_up_to_block_size(segment_capacity_request);
    if (!priv_reserve_vm(vm_region_size)) {
      priv_set_broken_status();
      return false;
    }
    m_segment = reinterpret_cast<char *>(m_vm_region) + header_size;
    m_segment_capacity =
        priv_round_down_to_block_size(m_vm_region_size - header_size);
    assert(m_segment_capacity >= segment_capacity_request);
    assert(m_segment_capacity + header_size <= m_vm_region_size);
    priv_construct_segment_header(m_vm_region);

    return true;
  }

  bool priv_reserve_vm(const std::size_t nbytes) {
    m_vm_region_size =
        mdtl::round_up((int64_t)nbytes, (int64_t)m_system_page_size);
    m_vm_region =
        mdtl::reserve_aligned_vm_region(m_system_page_size, m_vm_region_size);

    if (!m_vm_region) {
      std::stringstream ss;
      ss << "Cannot reserve a VM region " << m_vm_region_size << " bytes";
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      m_vm_region_size = 0;
      return false;
    } else {
      std::stringstream ss;
      ss << "Reserved a VM region: " << m_vm_region_size << " bytes at "
         << (uint64_t)m_vm_region;
      logger::out(logger::level::verbose, __FILE__, __LINE__, ss.str().c_str());
    }

    return true;
  }

  bool priv_release_vm_region() {
    // Overwrite the region with PROT_NONE to destroy the map. Because munmap(2)
    // synchronizes the map region with the file system, overwriting with
    // PROT_NONE beforehand makes munmap(2) fast. Ignore the success/failure of
    // this operation as this operation is just a performance optimization.
    mdtl::map_with_prot_none(m_segment, m_current_segment_size);

    if (!mdtl::munmap(m_vm_region, m_vm_region_size, false)) {
      std::stringstream ss;
      ss << "Cannot release a VM region " << (uint64_t)m_vm_region << ", "
         << m_vm_region_size << " bytes.";
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }
    m_vm_region = nullptr;
    m_vm_region_size = 0;

    return true;
  }

  bool priv_construct_segment_header(void *const addr) {
    if (!addr) {
      return false;
    }

    const auto size = mdtl::round_up(sizeof(segment_header_type),
                                     int64_t(m_system_page_size));
    if (mdtl::map_anonymous_write_mode(addr, size, MAP_FIXED) != addr) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Cannot allocate segment header");
      return false;
    }
    m_segment_header = reinterpret_cast<segment_header_type *>(addr);

    new (m_segment_header) segment_header_type();

    return true;
  }

  bool priv_deallocate_segment_header() {
    std::destroy_at(&m_segment_header);
    const auto size = mdtl::round_up(sizeof(segment_header_type),
                                     int64_t(m_system_page_size));
    const auto ret = mdtl::munmap(m_segment_header, size, false);
    m_segment_header = nullptr;
    if (!ret) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to deallocate segment header");
    }
    return ret;
  }

  bool priv_create(const path_type &top_path,
                   const std::size_t segment_capacity_request) {
    if (!check_sanity()) return false;
    if (is_open())
      return false;  // Cannot open multiple segments simultaneously.

    {
      std::string s("Create a segment under: " + top_path.string());
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    if (!mdtl::directory_exist(top_path)) {
      if (!mdtl::create_directory(top_path)) {
        std::string s("Cannot create a directory: " + top_path.string());
        logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
        // As no internal value has been changed, m_broken is still false.
        return false;
      }
    }

    if (!priv_prepare_header_and_segment(segment_capacity_request)) {
      priv_set_broken_status();
      return false;
    }

    m_top_path = top_path;
    m_read_only = false;

    // Create the first block so that we can assume that there is a block always
    // in a segment.
    if (!priv_create_new_map(m_top_path, 0, k_block_size, 0)) {
      priv_set_broken_status();
      return false;
    }
    m_current_segment_size = k_block_size;
    m_num_blocks = 1;

    if (!priv_test_file_space_free(top_path)) {
      std::string s("Failed to test file space free: " + top_path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_release_segment();
      priv_set_broken_status();
      return false;
    }

    return true;
  }

  bool priv_open(const path_type &top_path,
                 const std::size_t segment_capacity_request,
                 const bool read_only) {
    if (!check_sanity()) return false;
    if (is_open())
      return false;  // Cannot open multiple segments simultaneously.

    {
      std::string s("Open a segment under: " + top_path.string());
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    if (!priv_prepare_header_and_segment(
            read_only ? priv_get_size(top_path) : segment_capacity_request)) {
      priv_set_broken_status();
      return false;
    }

    m_top_path = top_path;
    m_read_only = read_only;

    // Maps block files one by one
    m_num_blocks = 0;
    while (true) {
      const auto file_name = priv_block_file_path(m_top_path, m_num_blocks);
      if (!mdtl::file_exist(file_name)) {
        break;  // Mapped all files
      }

      const auto file_size = mdtl::get_file_size(file_name);
      assert(file_size % page_size() == 0);
      if (k_block_size != (std::size_t)file_size) {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "File sizes are not the same");
        priv_release_segment();
        priv_set_broken_status();
        return false;
      }

      const auto fd =
          priv_map_file(file_name, k_block_size,
                        std::ptrdiff_t(m_current_segment_size), read_only);
      if (fd == -1) {
        std::stringstream ss;
        ss << "Failed to map a file " << file_name;
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
        priv_release_segment();
        priv_set_broken_status();
        return false;
      }
      m_block_fd_list.emplace_back(fd);
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
      m_anonymous_map_flag_list.push_back(false);
#endif
      m_current_segment_size += k_block_size;
      ++m_num_blocks;
    }

    if (!read_only && !priv_test_file_space_free(m_top_path)) {
      std::string s("Failed to test file space free: " + m_top_path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_release_segment();
      priv_set_broken_status();
      return false;
    }

    if (m_num_blocks == 0) {
      priv_set_broken_status();
      return false;
    }

    return true;
  }

  bool priv_extend(const std::size_t request_size) {
    if (!is_open()) return false;

    if (m_read_only) {
      return false;
    }

    if (request_size > m_segment_capacity) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Requested segment size is bigger than the reserved VM size");
      return false;
    }

    if (request_size <= m_current_segment_size) {
      return true;  // Already has enough segment size
    }

    while (m_current_segment_size < request_size) {
      if (!priv_create_new_map(m_top_path, m_num_blocks, k_block_size,
                               std::ptrdiff_t(m_current_segment_size))) {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "Failed to extend the segment");
        priv_release_segment();
        priv_set_broken_status();
        return false;
      }
      ++m_num_blocks;
      m_current_segment_size += k_block_size;
    }

    return true;
  }

  bool priv_create_new_map(const path_type &top_path,
                           const std::size_t block_number,
                           const std::size_t file_size,
                           const std::ptrdiff_t segment_offset) {
    const path_type file_name = priv_block_file_path(top_path, block_number);
    {
      std::string s("Create and extend a file " + file_name.string() +
                    " with " + std::to_string(file_size) + " bytes");
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    if (!mdtl::create_file(file_name)) return false;
    if (!mdtl::extend_file_size(file_name, file_size)) return false;
    if (static_cast<std::size_t>(mdtl::get_file_size(file_name)) < file_size) {
      std::string s("Failed to create and extend file: " + file_name.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    const auto fd = priv_map_anonymous(file_name, file_size, segment_offset);
    if (m_anonymous_map_flag_list.size() < block_number + 1) {
      m_anonymous_map_flag_list.resize(block_number + 1, false);
    }
    m_anonymous_map_flag_list[block_number] = true;
#else
    const auto fd = priv_map_file(file_name, file_size, segment_offset, false);
#endif
    if (fd == -1) {
      return false;
    }
    if (m_block_fd_list.size() < block_number + 1) {
      m_block_fd_list.resize(block_number + 1, -1);
    }
    m_block_fd_list[block_number] = fd;

    return true;
  }

  int priv_map_file(const path_type &path, const std::size_t file_size,
                    const std::ptrdiff_t segment_offset,
                    const bool read_only) const {
    assert(!path.empty());
    assert(file_size > 0);
    assert(segment_offset >= 0);
    assert(segment_offset + file_size <= m_segment_capacity);

    [[maybe_unused]] static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif

    const auto map_addr = static_cast<char *>(m_segment) + segment_offset;

    {
      std::stringstream ss;
      ss << "Map a file " << path << " at " << segment_offset << " with "
         << file_size << " bytes; read-only mode is "
         << std::to_string(read_only);
      logger::out(logger::level::verbose, __FILE__, __LINE__, ss.str().c_str());
    }

    const auto ret =
        (read_only)
            ? mdtl::map_file_read_mode(path, map_addr, file_size, 0, MAP_FIXED)
            : mdtl::map_file_write_mode(path, map_addr, file_size, 0,
                                        MAP_FIXED | map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map a file: " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) {
        mdtl::os_close(ret.first);
      }
      return -1;
    }

    return ret.first;
  }

  int priv_map_anonymous(const path_type &path, const std::size_t region_size,
                         const std::ptrdiff_t segment_offset) const {
    assert(!path.empty());
    assert(region_size > 0);
    assert(segment_offset >= 0);
    assert(segment_offset + region_size <= m_segment_capacity);

    const auto map_addr = static_cast<char *>(m_segment) + segment_offset;
    {
      std::string s("Map an anonymous region at " +
                    std::to_string(segment_offset) + " with " +
                    std::to_string(region_size));
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    const auto *addr =
        mdtl::map_anonymous_write_mode(map_addr, region_size, MAP_FIXED);
    if (!addr) {
      std::string s("Failed to map an anonymous region at " +
                    std::to_string(segment_offset));
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return -1;
    }

    // Although we do not map the file, we still open it so that other functions
    // in this class works.
    const auto fd = ::open(path.c_str(), O_RDWR);
    if (fd == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "open");
      std::string s("Failed to open a file " + path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      // Destroy the map by overwriting PROT_NONE map since the VM region is
      // managed by another class.
      mdtl::map_with_prot_none(static_cast<char *>(m_segment) + segment_offset,
                               region_size);
      return -1;
    }

    return fd;
  }

  bool priv_release_segment() {
    if (!is_open()) return false;

    int succeeded = true;
    for (const auto &fd : m_block_fd_list) {
      succeeded &= mdtl::os_close(fd);
    }

    succeeded &= priv_deallocate_segment_header();

    succeeded &= priv_release_vm_region();

    if (!succeeded) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to release the segment");
      priv_set_broken_status();
    } else {
      priv_clear_status();
    }

    return succeeded;
  }

  bool priv_sync(const bool sync) {
    if (!priv_sync_segment(
            sync)) {  // Failing this operation is not a critical error
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to synchronize the segment");
      return false;
    }
    return true;
  }

  bool priv_sync_segment(const bool sync) {
    if (!is_open()) return false;

    if (m_read_only) return true;

    // Protect the region to detect unexpected write by application during msync
    if (!mdtl::mprotect_read_only(m_segment, m_current_segment_size)) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to protect the segment with the read only mode");
      return false;
    }

    logger::out(logger::level::verbose, __FILE__, __LINE__,
                "msync() for the application data segment");
    if (!priv_parallel_msync(sync)) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to msync the segment");
      return false;
    }
    if (!mdtl::mprotect_read_write(m_segment, m_current_segment_size)) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to set the segment to readable and writable");
      return false;
    }

    return true;
  }

  bool priv_parallel_msync(const bool sync) {
    std::atomic_uint_fast64_t block_no_count = 0;
    std::atomic_uint_fast64_t num_successes = 0;
    auto diff_sync = [&sync, &block_no_count, &num_successes, this]() {
      while (true) {
        const auto block_no = block_no_count.fetch_add(1);
        if (block_no < m_block_fd_list.size()) {
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
          assert(m_anonymous_map_flag_list.size() > block_no);
          if (m_anonymous_map_flag_list[block_no]) {
            num_successes.fetch_add(priv_sync_anonymous_map(block_no) ? 1 : 0);
            continue;
          }
#endif
          const auto map =
              static_cast<char *>(m_segment) + block_no * k_block_size;
          num_successes.fetch_add(mdtl::os_msync(map, k_block_size, sync) ? 1
                                                                          : 0);
        } else {
          break;
        }
      }
    };

    const auto num_threads =
        (int)std::min(m_block_fd_list.size(),
                      (std::size_t)std::thread::hardware_concurrency());
    {
      std::stringstream ss;
      ss << "Sync files with " << num_threads << " threads";
      logger::out(logger::level::verbose, __FILE__, __LINE__, ss.str().c_str());
    }
    std::vector<std::unique_ptr<std::thread>> threads(num_threads);
    for (auto &th : threads) {
      th = std::make_unique<std::thread>(diff_sync);
    }

    for (auto &th : threads) {
      th->join();
    }

    return num_successes == m_block_fd_list.size();
  }

  bool priv_free_region(const std::ptrdiff_t offset,
                        const std::size_t nbytes) const {
    if (!is_open() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

#ifdef METALL_USE_ANONYMOUS_NEW_MAP
    const auto block_no = offset / k_block_size;
    assert(m_anonymous_map_flag_list.size() > block_no);
    if (m_anonymous_map_flag_list[block_no]) {
      return priv_uncommit_private_anonymous_pages(offset, nbytes);
    }
#endif

    if (m_free_file_space)
      return priv_uncommit_pages_and_free_file_space(offset, nbytes);
    else
      return priv_uncommit_pages(offset, nbytes);
  }

  bool priv_uncommit_pages_and_free_file_space(const std::ptrdiff_t offset,
                                               const std::size_t nbytes) const {
    return mdtl::uncommit_shared_pages_and_free_file_space(
        static_cast<char *>(m_segment) + offset, nbytes);
  }

  bool priv_uncommit_pages(const std::ptrdiff_t offset,
                           const std::size_t nbytes) const {
    return mdtl::uncommit_shared_pages(static_cast<char *>(m_segment) + offset,
                                       nbytes);
  }

  bool priv_uncommit_private_anonymous_pages(const std::ptrdiff_t offset,
                                             const std::size_t nbytes) const {
    return mdtl::uncommit_private_anonymous_pages(
        static_cast<char *>(m_segment) + offset, nbytes);
  }

#ifdef METALL_USE_ANONYMOUS_NEW_MAP
  bool priv_sync_anonymous_map(const std::size_t block_no) {
    assert(m_anonymous_map_flag_list[block_no]);
    {
      std::string s("Sync anonymous map at block " + std::to_string(block_no));
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    auto *const addr = static_cast<char *>(m_segment) + block_no * k_block_size;
    if (::write(m_block_fd_list[block_no], addr, k_block_size) !=
        (ssize_t)k_block_size) {
      std::string s("Failed to write back a block");
      logger::perror(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_release_segment();
      priv_set_broken_status();
      return false;
    }
    m_anonymous_map_flag_list[block_no] = false;

    {
      std::string s("Map block " + std::to_string(block_no) +
                    " as a non-anonymous map");
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }
    [[maybe_unused]] static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif
    const auto mapped_addr =
        mdtl::map_file_write_mode(m_block_fd_list[block_no], addr, k_block_size,
                                  0, MAP_FIXED | map_nosync);
    if (!mapped_addr || mapped_addr != addr) {
      std::string s("Failed to map a block");
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      priv_release_segment();
      priv_set_broken_status();
      return false;
    }
    return true;
  }
#endif

  bool priv_set_system_page_size() {
    m_system_page_size = mdtl::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  bool priv_test_file_space_free(const path_type &top_path) {
#ifdef METALL_DISABLE_FREE_FILE_SPACE
    m_free_file_space = false;
    return true;
#endif

    assert(m_system_page_size > 0);
    const path_type file_path(top_path.string() + "/test");
    const std::size_t file_size = m_system_page_size * 2;

    if (!mdtl::create_file(file_path)) return false;
    if (!mdtl::extend_file_size(file_path, file_size)) return false;
    assert(static_cast<std::size_t>(mdtl::get_file_size(file_path)) >=
           file_size);

    const auto ret =
        mdtl::map_file_write_mode(file_path, nullptr, file_size, 0);
    if (ret.first == -1 || !ret.second) {
      std::string s("Failed to map file: " + file_path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      if (ret.first != -1) mdtl::os_close(ret.first);
      return false;
    }

    // Test freeing file space
    char *buf = static_cast<char *>(ret.second);
    buf[0] = 0;
    if (mdtl::uncommit_shared_pages_and_free_file_space(ret.second,
                                                        file_size)) {
      m_free_file_space = true;
    } else {
      m_free_file_space = false;
    }

    if (!mdtl::os_close(ret.first)) {
      std::string s("Failed to close file: " + file_path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    // Closing
    mdtl::munmap(ret.second, file_size, false);
    if (!mdtl::remove_file(file_path)) {
      std::string s("Failed to remove a file: " + file_path.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    {
      std::string s("File free test result: ");
      s += m_free_file_space ? "success" : "failed";
      logger::out(logger::level::verbose, __FILE__, __LINE__, s.c_str());
    }

    return true;
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  ssize_t m_system_page_size{0};
  std::size_t m_num_blocks{0};
  std::size_t m_vm_region_size{0};
  std::size_t m_segment_capacity{0};
  std::size_t m_current_segment_size{0};
  void *m_vm_region{nullptr};
  void *m_segment{nullptr};
  segment_header_type *m_segment_header{nullptr};
  path_type m_top_path;
  bool m_read_only{false};
  bool m_free_file_space{true};
  std::vector<int> m_block_fd_list;
  bool m_broken{false};
#ifdef METALL_USE_ANONYMOUS_NEW_MAP
  std::vector<int> m_anonymous_map_flag_list;
#endif
};

}  // namespace metall::kernel

#endif  // METALL_KERNEL_SEGMENT_STORAGE_HPP
