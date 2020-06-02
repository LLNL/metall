// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_SEGMENT_STORAGE_UMAP_STORAGE_HPP
#define METALL_DETAIL_V0_SEGMENT_STORAGE_UMAP_STORAGE_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/stat.h>
#include <fcntl.h>

#include <umap/umap.h>

#include <string>
#include <iostream>
#include <cassert>
#include <metall/detail/utility/file.hpp>
#include <metall/detail/utility/mmap.hpp>

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

template <typename different_type, typename size_type>
class umap_segment_storage {

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  umap_segment_storage()
      : m_umap_page_size(0),
        m_num_blocks(0),
        m_vm_region_size(0),
        m_current_segment_size(0),
        m_segment(nullptr),
        m_base_path(),
        m_read_only(),
        m_free_file_space(true) {
    if (!priv_load_umap_page_size()) {
      std::abort();
    }
  }

  ~umap_segment_storage() {
    // FIXME: turn off for now
    // sync(true);
    destroy();
  }

  umap_segment_storage(const umap_segment_storage &) = delete;
  umap_segment_storage &operator=(const umap_segment_storage &) = delete;

  umap_segment_storage(umap_segment_storage &&other) noexcept :
      m_umap_page_size(other.m_umap_page_size),
      m_num_blocks(other.m_num_blocks),
      m_vm_region_size(other.m_vm_region_size),
      m_current_segment_size(other.m_current_segment_size),
      m_segment(other.m_segment),
      m_base_path(other.m_base_path),
      m_read_only(other.m_read_only),
      m_free_file_space(other.m_free_file_space) {
    other.priv_reset();
  }

  umap_segment_storage &operator=(umap_segment_storage &&other) noexcept {
    m_umap_page_size = other.m_umap_page_size;
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

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief Check if there is a file that can be opened
  static bool openable(const std::string &base_path) {
    const auto file_name = priv_make_file_name(base_path, 0);
    return util::file_exist(file_name);
  }

  /// \brief Gets the size of an existing segment
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
        break;
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

    return (m_num_blocks > 0);
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
    return m_umap_page_size;
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
    m_umap_page_size = 0;
    m_num_blocks = 0;
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_segment = nullptr;
    // m_read_only = false;
  }

  bool priv_inited() const {
    return (m_umap_page_size > 0 && m_num_blocks > 0 && m_vm_region_size > 0 && m_current_segment_size > 0
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
    if (static_cast<size_type>(util::get_file_size(file_name)) < file_size) {
      std::cerr << "Failed to create and extend file: " << file_name << std::endl;
      std::abort();
    }


    if (!priv_map_file(file_name, file_size, addr, false)) {
      return false;
    }
    return true;
  }

  bool priv_map_file(const std::string &path, const size_type file_size, void *const addr, const bool read_only) const {
    assert(!path.empty());
    assert(file_size > 0);
    assert(addr);

    // MEMO: one of the following options does not work on /tmp?
    const int o_opts = (read_only ? O_RDONLY : O_RDWR) | O_LARGEFILE | O_DIRECT;
    const int fd = ::open(path.c_str(), o_opts);
    if (fd == -1) {
      std::string estr = "Failed to open " + path;
      perror(estr.c_str());
      return false;
    }

    const int prot = PROT_READ | (read_only ? 0 : PROT_WRITE);
    const int flags = UMAP_PRIVATE | MAP_FIXED;
    void *const region = ::umap(addr, file_size, prot, flags, fd, 0);
    if (region == UMAP_FAILED) {
      std::ostringstream ss;
      ss << "umap_mf of " << file_size << " bytes failed for " << path;
      perror(ss.str().c_str());
      return false;
    }

    return true;
  }

  void priv_unmap_all_files() {

    size_type offset = 0;
    for (size_type n = 0; n < m_num_blocks; ++n) {
      const auto file_name = priv_make_file_name(m_base_path, n);
      assert(util::file_exist(file_name));

      const auto file_size = util::get_file_size(file_name);
      assert(file_size % page_size() == 0);

      if (::uunmap(static_cast<char *>(m_segment) + offset, file_size) != 0) {
        std::cerr << "Failed to unmap a Umap region"
                  << "\nblock number " << n
                  << "\noffset " << offset << std::endl;
        std::abort();
      }
      offset += file_size;
    }
    assert(offset == m_current_segment_size);

    m_num_blocks = 0;
    m_current_segment_size = 0;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    priv_unmap_all_files();

    priv_reset();
  }

  void priv_sync_segment([[maybe_unused]] const bool sync) {
    if (!priv_inited() || m_read_only) return;

    if (::umap_flush() != 0) {
      std::cerr << "Failed umap_flush()" << std::endl;
    }
  }

  // TODO: implement
  bool priv_free_region(const different_type offset, const size_type nbytes) {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_current_segment_size) return false;

//   if (m_free_file_space) {
//     util::free_mmap_region();
//   }

    return true;
  }

  bool priv_load_umap_page_size() {
    m_umap_page_size = ::umapcfg_get_umap_page_size();
    if (m_umap_page_size == -1) {
      std::cerr << "Failed to get system pagesize" << std::endl;
      return false;
    }
    return true;
  }

  void priv_test_file_space_free(const std::string &) {
    m_free_file_space = false;
    return;
  }

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  ssize_t m_umap_page_size{0};
  size_type m_num_blocks{0};
  size_type m_vm_region_size{0};
  size_type m_current_segment_size{0};
  void *m_segment{nullptr};
  std::string m_base_path;
  bool m_read_only;
  bool m_free_file_space{true};
};

} // namespace kernel
} // namespace metall

#endif //METALL_DETAIL_V0_SEGMENT_STORAGE_UMAP_STORAGE_HPP