// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_SEGMENT_STORAGE_HPP
#define METALL_DETAIL_V0_SEGMENT_STORAGE_HPP

#include <string>
#include <iostream>
#include <cassert>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <typename offset_type, typename size_type, std::size_t k_header_size>
class file_mapped_segment_storage {

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  file_mapped_segment_storage() = default;

  ~file_mapped_segment_storage() {
    sync();
    destroy();
  }

  file_mapped_segment_storage(const file_mapped_segment_storage &) = delete;
  file_mapped_segment_storage &operator=(const file_mapped_segment_storage &) = delete;

  file_mapped_segment_storage(file_mapped_segment_storage &&other) noexcept :
      m_fd(other.m_fd),
      m_header(other.m_header),
      m_segment(other.m_segment),
      m_segment_size(other.m_segment_size) {
    other.priv_reset();
  }

  file_mapped_segment_storage &operator=(file_mapped_segment_storage &&other) noexcept {
    m_fd = other.m_fd;
    m_header = other.m_header;
    m_segment = other.m_segment;
    m_segment_size = other.m_segment_size;

    other.priv_reset();

    return (*this);
  }

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  bool create(const char *path, const size_type segment_size) {
    assert(!priv_mapped());

    if (!util::create_file(path)) return false;
    if (!util::extend_file_size(path, segment_size)) return false;

    assert(static_cast<size_type>(util::get_file_size(path)) == segment_size);

    return priv_allocate_header_and_map_segment(path);
  }

  bool open(const char *path) {
    assert(!priv_mapped());

    if (!util::file_exist(path)) return false;

    return priv_allocate_header_and_map_segment(path);
  }

  void destroy() {
    priv_destroy_header_and_segment();
  }

  void sync() {
    priv_sync_segment();
  }

  void free_region(const offset_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  void *get_header() const {
    return m_header;
  }

  void *get_segment() const {
    return m_segment;
  }

  size_type size() const {
    return m_segment_size;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------------------------------------------------------------------- //
  void priv_reset() {
    m_fd = -1;
    m_header = nullptr;
    m_segment = nullptr;
    m_segment_size = 0;
  }

  bool priv_mapped() const {
    return (m_header && m_segment && m_segment_size > 0); // does not check m_fd on purpose
  }

  bool priv_allocate_header_and_map_segment(const char *const path) {
    const auto file_size = util::get_file_size(path);
    if (file_size <= 0) {
      std::cerr << "The backing file's size is invalid: " << file_size << std::endl;
      priv_reset();
      return false;
    }

    char *const addr = reinterpret_cast<char *>(priv_reserve_vm_address(k_header_size + file_size));
    if (!addr) return false;

    return priv_allocate_header(addr) && priv_map_segment(path, addr + k_header_size, file_size);
  }

  void *priv_reserve_vm_address(const size_t nbytes) {
    void *const addr = util::reserve_vm_region(nbytes);
    if (!addr) {
      std::cerr << "Cannot reserve a VM region " << nbytes << std::endl;
    }
    return addr;
  }

  bool priv_allocate_header(void *const addr) {
    assert(addr);

    m_header = addr;
    if (util::map_anonymous_write_mode(m_header, k_header_size, MAP_FIXED) != m_header) {
      std::cerr << "Cannot allocate the segment header" << std::endl;
      return false;
    }
    return true;
  }

  bool priv_map_segment(const char *const path, void *const addr, const size_type file_size) {
    assert(addr);
    assert(!priv_mapped());

    static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif
    const auto ret = util::map_file_write_mode(path, addr, file_size, 0, MAP_FIXED | map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::cerr << "Failed mmap" << std::endl;
      priv_reset();
      return false;
    }

    m_fd = ret.first;
    m_segment = ret.second;
    m_segment_size = file_size;

    return true;
  }

  void priv_destroy_header_and_segment() {
    if (!priv_mapped()) return;

    util::munmap(m_header, k_header_size, false);
    util::map_with_prot_none(m_segment, m_segment_size);
    util::munmap(m_fd, m_segment, m_segment_size, false);

    priv_reset();
  }

  void priv_sync_segment() {
    if (!priv_mapped()) return;

    util::os_msync(m_segment, m_segment_size);
    util::os_fsync(m_fd);
  }

  void priv_free_region(const offset_type offset, const size_type nbytes) {
    if (!priv_mapped()) return;

    if (offset + nbytes > m_segment_size) return;

    util::deallocate_file_space(m_fd, offset, nbytes);
    util::uncommit_shared_pages(static_cast<char *>(m_segment) + offset, nbytes);
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  int m_fd{-1};
  void *m_header{nullptr};
  void *m_segment{nullptr};
  size_type m_segment_size{0};
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_SEGMENT_STORAGE_HPP
