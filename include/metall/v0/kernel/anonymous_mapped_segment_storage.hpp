// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_ANONYMOUS_MAPPED_SEGMENT_STORAGE_HPP
#define METALL_DETAIL_V0_KERNEL_ANONYMOUS_MAPPED_SEGMENT_STORAGE_HPP

#include <unistd.h>

#include <iostream>
#include <fstream>
#include <string>

#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <typename offset_type, typename size_type, std::size_t header_size>
class anonymous_mapped_segment_storage {

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  anonymous_mapped_segment_storage() = default;

  ~anonymous_mapped_segment_storage() {
    priv_sync_segment();
    priv_unmap_segment();
  }

  anonymous_mapped_segment_storage(const anonymous_mapped_segment_storage &) = delete;
  anonymous_mapped_segment_storage &operator=(const anonymous_mapped_segment_storage &) = delete;

  anonymous_mapped_segment_storage(anonymous_mapped_segment_storage &&other) noexcept :
      m_fd(other.m_fd),
      m_header(other.m_header),
      m_segment(other.m_segment),
      m_segment_size(other.m_segment_size) {
    other.priv_reset();
  }

  anonymous_mapped_segment_storage &operator=(anonymous_mapped_segment_storage &&other) noexcept {
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
  bool create(const char *path, const size_type nbytes) {
    assert(!priv_mapped());

    if (!util::create_file(path)) return false;
    if (!util::extend_file_size(path, nbytes)) return false;

    assert(util::get_file_size(path) == nbytes);

    m_fd = ::open(path, O_RDWR);
    if (m_fd == -1) {
      ::perror("open");
      std::cerr << "errno: " << errno << std::endl;
      return false;
    }

    if (!priv_allocate_header_and_segment(nbytes)) return false;

    return true;
  }

  bool open(const char *path) {
    assert(!priv_mapped());

    if (!util::file_exist(path)) return false;

    m_fd = ::open(path, O_RDWR);
    if (m_fd == -1) {
      ::perror("open");
      std::cerr << "errno: " << errno << std::endl;
      return false;
    }

    if (!priv_allocate_header_and_segment(util::get_file_size(path))) return false;

    if (!priv_read_file()) return false;

    return true;
  }

  void destroy() {
    priv_unmap_segment();
  }

  void sync([[maybe_unused]] const bool sync) {
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

  bool priv_allocate_header_and_segment(const size_type size) {
    assert(!priv_mapped());

    if (size <= 0) return false;
    m_segment_size = size;

    char* addr = reinterpret_cast<char*>(util::map_anonymous_write_mode(nullptr, header_size + m_segment_size, 0));
    if (!addr) {
      std::cerr << "Failed to allocate header and segment" << std::endl;
      priv_reset();
      return false;
    }

    m_header = addr;
    m_segment = addr + header_size;

    return true;
  }

  bool priv_read_file() {
    assert(m_segment);
    assert(m_segment_size > 0);

    const auto ret = ::pread(m_fd, m_segment, m_segment_size, 0);
    if (ret != m_segment_size) {
      if (ret == -1) ::perror("read");
      std::cerr << "Failed to read data " << m_segment_size << " != " << ret << std::endl;
      return false;
    }

    return true;
  }

  void priv_unmap_segment() {
    if (!priv_mapped()) return;

    util::munmap(m_fd, m_header, header_size + m_segment_size, false);
    priv_reset();
  }

  void priv_sync_segment() {
    if (!priv_mapped() || m_fd <= 0) return;

    const auto ret = ::pwrite(m_fd, m_segment, m_segment_size, 0);
    if (ret != m_segment_size) {
      if (ret == -1) ::perror("write");
      std::cerr << "Failed to write data " << m_segment_size << " != " << ret << std::endl;
    }

    if (::fsync(m_fd) == -1) {
      ::perror("fsync");
      std::cerr << "Failed to synch with the file" << std::endl;
    }
  }

  void priv_free_region(const offset_type offset, const size_type nbytes) {
    if (!priv_mapped()) return;

    if (offset + nbytes > m_segment_size) return;

    util::uncommit_private_pages(static_cast<char *>(m_segment) + offset, nbytes);
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

#endif //METALL_DETAIL_V0_KERNEL_ANONYMOUS_MAPPED_SEGMENT_STORAGE_HPP
