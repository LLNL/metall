// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_SEGMENT_STORAGE_MULTIFILE_BACKED_STORAGE_HPP
#define METALL_DETAIL_V0_SEGMENT_STORAGE_MULTIFILE_BACKED_STORAGE_HPP

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

template <typename different_type, typename size_type>
class multifile_backed_segment_storage {

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  multifile_backed_segment_storage()
      : m_system_page_size(0),
        m_num_blocks(0),
        m_vm_region_size(0),
        m_current_segment_size(0),
        m_segment(nullptr),
        m_base_path(),
        m_read_only(),
        m_free_file_space(true) {
    if (!load_system_page_size()) {
      std::abort();
    }
  }

  ~multifile_backed_segment_storage() {
    sync(true);
    destroy();
  }

  multifile_backed_segment_storage(const multifile_backed_segment_storage &) = delete;
  multifile_backed_segment_storage &operator=(const multifile_backed_segment_storage &) = delete;

  multifile_backed_segment_storage(multifile_backed_segment_storage &&other) noexcept :
      m_system_page_size(other.m_system_page_size),
      m_num_blocks(other.m_num_blocks),
      m_vm_region_size(other.m_vm_region_size),
      m_current_segment_size(other.m_current_segment_size),
      m_segment(other.m_segment),
      m_base_path(other.m_base_path),
      m_read_only(other.m_read_only),
      m_free_file_space(other.m_free_file_space) {
    other.priv_reset();
  }

  multifile_backed_segment_storage &operator=(multifile_backed_segment_storage &&other) noexcept {
    m_system_page_size = other.m_system_page_size;
    m_num_blocks = other.m_num_blocks;
    m_vm_region_size = other.m_vm_region_size;
    m_current_segment_size = other.m_current_segment_size;
    m_segment = other.m_segment;
    m_base_path = other.m_base_path;
    m_read_only = other.m_read_only;
    m_free_file_space = other.m_free_file_space;

    other.priv_reset();

    return (*this);
  }

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  bool create(const std::string &base_path,
              const size_type vm_region_size,
              void *const vm_region,
              const size_type initial_segment_size) {
    assert(!priv_inited());


    // TODO: align those values to pge size
    if (initial_segment_size % page_size() != 0 || vm_region_size % page_size() != 0
        || (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to crete application data segment" << std::endl;
      std::abort();
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = false;

    const auto segment_size = std::min(vm_region_size, initial_segment_size);
    if (!priv_create_and_map_file(m_base_path, 0, segment_size, m_segment)) {
      priv_reset();
      return false;
    }
    m_current_segment_size += segment_size;
    m_num_blocks = 1;

    priv_test_file_space_free(base_path);

    return true;
  }

  bool open(const std::string &base_path, const size_type vm_region_size, void *const vm_region, const bool read_only) {
    assert(!priv_inited());

    // TODO: align those values to pge size
    if (vm_region_size % page_size() != 0 || (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to open segment" << std::endl;
      std::abort(); // Fatal error
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = read_only;

    m_num_blocks = 0;
    while (true) {
      const auto file_name = priv_make_file_name(m_base_path, m_num_blocks);
      if (!util::file_exist(file_name)) {
        return (m_num_blocks > 0);
      }

      const auto file_size = util::get_file_size(file_name);
      assert(file_size % page_size() == 0);
      if (!priv_map_file(file_name, file_size, static_cast<char *>(m_segment) + m_current_segment_size, read_only)) {
        std::abort(); // Fatal error
      }
      m_current_segment_size += file_size;
      ++m_num_blocks;
    }

    if (!read_only) {
      priv_test_file_space_free(base_path);
    }

    assert(false);
  }

  bool extend(const size_type new_segment_size) {
    assert(priv_inited());

    if (m_read_only) {
      return false;
    }

    if (new_segment_size > m_vm_region_size) {
      std::cerr << "Requested segment size is too big" << std::endl;
      return false;
    }

    if (new_segment_size <= m_current_segment_size) {
      return true; // Already enough segment size
    }

    if (!priv_create_and_map_file(m_base_path,
                                  m_num_blocks,
                                  new_segment_size - m_current_segment_size,
                                  static_cast<char *>(m_segment) + m_current_segment_size)) {
      priv_reset();
      return false;
    }
    ++m_num_blocks;
    m_current_segment_size = new_segment_size;

    return true;
  }

  void destroy() {
    priv_destroy_segment();
  }

  void sync(const bool sync) {
    priv_sync_segment(sync);
  }

  void free_region(const different_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

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
                                void *const addr) const {
    assert(!m_segment || static_cast<char *>(m_segment) + m_current_segment_size <= addr);

    const std::string file_name = priv_make_file_name(base_path, block_number);
    if (!util::create_file(file_name)) return false;
    if (!util::extend_file_size(file_name, file_size)) return false;
    assert(static_cast<size_type>(util::get_file_size(file_name)) >= file_size);

    if (!priv_map_file(file_name, file_size, addr, false)) {
      return false;
    }
    return true;
  }

  bool priv_map_file(const std::string &path, const size_type file_size, void *const addr, const bool read_only) const {
    assert(!path.empty());
    assert(file_size > 0);
    assert(addr);

    static constexpr int map_nosync =
#ifdef MAP_NOSYNC
        MAP_NOSYNC;
#else
        0;
#endif

    const auto ret = (read_only) ?
                     util::map_file_read_mode(path, addr, file_size, 0, MAP_FIXED) :
                     util::map_file_write_mode(path, addr, file_size, 0, MAP_FIXED | map_nosync);
    if (ret.first == -1 || !ret.second) {
      std::cerr << "Failed to map a file: " << path << std::endl;
      if (ret.first == -1) ::close(ret.first);
      return false;
    }

    ::close(ret.first);

    return true;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    util::map_with_prot_none(m_segment, m_current_segment_size);
    // util::munmap(m_segment, m_current_segment_size, false); // VM region will be unmapped by manager_kernel

    priv_reset();
  }

  void priv_sync_segment(const bool sync) {
    if (!priv_inited() || m_read_only) return;

    util::os_msync(m_segment, m_current_segment_size, sync);
    // util::os_fsync(m_fd);
  }

  bool priv_free_region(const different_type offset, const size_type nbytes) {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

    if (m_free_file_space)
      return util::uncommit_file_backed_pages(static_cast<char *>(m_segment) + offset, nbytes);
    else
      return util::uncommit_shared_pages(static_cast<char *>(m_segment) + offset, nbytes);
  }

  bool load_system_page_size() {
    m_system_page_size = util::get_page_size();
    if (m_system_page_size == -1) {
      std::cerr << "Failed to get system pagesize" << std::endl;
      return false;
    }
    return true;
  }

  void priv_test_file_space_free(const std::string &base_path) {
    assert(m_system_page_size > 0);
    const std::string file_path(base_path + "_test");
    const size_type file_size = m_system_page_size * 2;

    if (!util::create_file(file_path)) return;
    if (!util::extend_file_size(file_path, file_size)) return;
    assert(static_cast<size_type>(util::get_file_size(file_path)) >= file_size);

    const auto ret = util::map_file_write_mode(file_path, nullptr, file_size, 0);
    if (ret.first == -1 || !ret.second) {
      std::cerr << "Failed to map a file: " << file_path << std::endl;
      if (ret.first == -1) ::close(ret.first);
      return;
    }
    ::close(ret.first);

    // Test freeing file space
    char* buf = static_cast<char*>(ret.second);
    buf[0] = 0;
    if (util::uncommit_file_backed_pages(ret.second, file_size)) {
      m_free_file_space = true;
    } else {
      m_free_file_space = false;
    }

    // Closing
    util::munmap(ret.second, file_size, false);
    if (!util::remove_file(file_path) ){
      std::cerr << "Failed to remove a file: " << file_path << std::endl;
      return;
    }
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  ssize_t m_system_page_size{0};
  size_type m_num_blocks{0};
  size_type m_vm_region_size{0};
  size_type m_current_segment_size{0};
  void *m_segment{nullptr};
  std::string m_base_path;
  bool m_read_only;
  bool m_free_file_space{true};
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_SEGMENT_STORAGE_MULTIFILE_BACKED_STORAGE_HPP
