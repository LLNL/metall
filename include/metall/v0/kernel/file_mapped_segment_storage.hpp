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

template <typename offset_type, typename size_type>
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
    priv_sync_segment();
    priv_unmap_segment();
  }

  file_mapped_segment_storage(const file_mapped_segment_storage &) = delete;
  file_mapped_segment_storage &operator=(const file_mapped_segment_storage &) = delete;

  file_mapped_segment_storage(file_mapped_segment_storage &&other) noexcept :
      m_fd(other.m_fd),
      m_segment(other.m_segment),
      m_segment_size(other.m_segment_size) {
    other.priv_reset();
  }

  file_mapped_segment_storage &operator=(file_mapped_segment_storage &&other) noexcept {
    m_fd = other.m_fd;
    m_segment = other.m_segment;
    m_segment_size = other.m_segment_size;

    other.priv_reset();

    return (*this);
  }

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  bool create(const char *path, const size_type nbytes) {
    assert(!priv_mapped());

    if (!util::create_file(path)) return false;
    if (!util::extend_file_size(path, nbytes)) return false;

    assert(util::get_file_size(path) == nbytes);

    return priv_map_segment(path);
  }

  bool open(const char *path) {
    assert(!priv_mapped());

    if (!util::file_exist(path)) return false;

    return priv_map_segment(path);
  }

  void destroy() {
    priv_unmap_segment();
  }

  void sync() {
    priv_sync_segment();
  }

  void free_region(const offset_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  void *segment() const {
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
    m_segment = nullptr;
    m_segment_size = 0;
  }

  bool priv_mapped() const {
    return (m_segment && m_segment_size > 0); // does not check m_fd on purpose
  }

  void *priv_reserve_vm_address(const size_t nbytes) {
    void *const addr = util::reserve_vm_region(nbytes);
    if (!addr) {
      std::cerr << "Cannot reserve a VM region " << nbytes << std::endl;
    }
    return addr;
  }

  bool priv_map_segment(const char *path) {
    assert(!priv_mapped());

    const auto size = util::get_file_size(path);
    if (size <= 0) {
      std::cerr << "The backing file's size is invalid: " << size << std::endl;
      priv_reset();
      return false;
    }
    m_segment_size = static_cast<size_type>(size);

//    m_segment = priv_reserve_vm_address(m_segment_size);
//    if (!m_segment) {
//      priv_reset();
//      return false;
//    }
//
//    const auto ret = util::map_file_write_mode(path, m_segment, m_segment_size, 0, MAP_FIXED);
//    if (ret.first == -1 || ret.second != m_segment) {
//      std::cerr << "Failed mmap" << std::endl;
//      priv_reset();
//      return false;
//    }

    const int additional_map_flag =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif
    const auto ret = util::map_file_write_mode(path, nullptr, m_segment_size, 0, additional_map_flag);
    if (ret.first == -1 || !ret.second) {
      std::cerr << "Failed mmap" << std::endl;
      priv_reset();
      return false;
    }

    m_fd = ret.first;
    m_segment = ret.second;

    return true;
  }

  void priv_unmap_segment() {
    if (!priv_mapped()) return;

    util::map_with_prot_none(m_segment, m_segment_size);
    util::munmap(m_fd, m_segment, m_segment_size, false);

    priv_reset();
  }

  void priv_sync_segment() {
    if (!priv_mapped()) return;

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
  void *m_segment{nullptr};
  size_type m_segment_size{0};
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_SEGMENT_STORAGE_HPP
