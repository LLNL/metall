// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAILL_SEGMENT_STORAGE_UMAP_SPARSE_SEGMENT_STORAGE_HPP
#define METALL_DETAILL_SEGMENT_STORAGE_UMAP_SPARSE_SEGMENT_STORAGE_HPP

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

// UMap SparseStore file granularity
const size_t SPARSE_STORE_FILE_GRANULARITY_DEFAULT = 1073741824;

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <umap/umap.h>
#include <umap/store/SparseStore.h>

#include <string>
#include <iostream>
#include <cassert>
#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/utilities.hpp>

// #include "umap_sparse_store.hpp"

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
}

template <typename different_type, typename size_type>
class umap_sparse_segment_storage {
 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  umap_sparse_segment_storage()
      : m_umap_page_size(0),
        m_vm_region_size(0),
        m_segment_size(0),
        m_segment(nullptr),
        m_base_path(),
        m_read_only(),
        m_free_file_space(true) {
    if (!priv_load_umap_page_size()) {
      std::abort();
    }
  }

  ~umap_sparse_segment_storage() {
    priv_sync_segment(true);
    destroy();
  }

  umap_sparse_segment_storage(const umap_sparse_segment_storage &) = delete;
  umap_sparse_segment_storage &operator=(const umap_sparse_segment_storage &) =
      delete;

  umap_sparse_segment_storage(umap_sparse_segment_storage &&other) noexcept
      : m_umap_page_size(other.m_umap_page_size),
        m_vm_region_size(other.m_vm_region_size),
        m_segment_size(other.m_segment_size),
        m_segment(other.m_segment),
        m_base_path(other.m_base_path),
        m_read_only(other.m_read_only),
        m_free_file_space(other.m_free_file_space) {
    other.priv_reset();
  }

  umap_sparse_segment_storage &operator=(
      umap_sparse_segment_storage &&other) noexcept {
    m_umap_page_size = other.m_umap_page_size;
    m_vm_region_size = other.m_vm_region_size;
    m_segment_size = other.m_segment_size;
    m_segment = other.m_segment;
    m_base_path = other.m_base_path;
    m_read_only = other.m_read_only;
    m_free_file_space = other.m_free_file_space;

    other.priv_reset();

    return (*this);
  }

  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief Check if there is a file that can be opened
  static bool openable(const std::string &base_path) {
    return mdtl::file_exist(priv_make_file_name(base_path));
  }

  /// \brief Gets the size of an existing segment.
  /// This is a static version of size() method.
  static size_type get_size(const std::string &base_path) {
    const auto directory_name = priv_make_file_name(base_path);
    return Umap::SparseStore::get_capacity(directory_name);
  }

  /// \brief Copies segment to another location.
  static bool copy(const std::string &source_path,
                   const std::string &destination_path, const bool clone,
                   [[maybe_unused]] const int max_num_threads) {
    // TODO: implement parallel copy version

    if (clone) {
      std::string s("Clone: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::clone_file(source_path, destination_path);
    } else {
      std::string s("Copy: " + source_path);
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::copy_file(source_path, destination_path);
    }
    assert(false);
    return false;
  }

  /// \brief Creates a new segment by mapping file(s) to the given VM address.
  /// \param base_path A path to create a datastore.
  /// \param vm_region_size The size of the VM region.
  /// \param vm_region The address of the VM region.
  /// \param initial_segment_size_hint Not used.
  /// \return Returns true on success; otherwise, false.
  bool create(const std::string &base_path, const size_type vm_region_size,
              void *const vm_region,
              [[maybe_unused]] const size_type initial_segment_size_hint) {
    assert(!priv_inited());

    // TODO: align those values to the page size instead of aborting
    if (vm_region_size % page_size() != 0 ||
        (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to crete application data segment"
                << std::endl;
      std::abort();
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = false;

    const auto segment_size = vm_region_size;
    if (!priv_create_and_map_file(m_base_path, segment_size, m_segment)) {
      priv_reset();
      return false;
    }
    m_segment_size += segment_size;

    priv_test_file_space_free(base_path);

    return true;
  }

  /// \brief Opens an existing Metall datastore.
  /// \param base_path The path to datastore.
  /// \param vm_region_size The size of the VM region.
  /// \param vm_region The address of the VM region.
  /// \param read_only If this option is true, opens the datastore with read
  /// only mode. \return Returns true on success; otherwise, false.
  bool open(const std::string &base_path, const size_type vm_region_size,
            void *const vm_region, const bool read_only) {
    assert(!priv_inited());
    // TODO: align those values to pge size
    if (vm_region_size % page_size() != 0 ||
        (uint64_t)vm_region % page_size() != 0) {
      std::cerr << "Invalid argument to open segment" << std::endl;
      std::abort();  // Fatal error
    }

    m_base_path = base_path;
    m_vm_region_size = vm_region_size;
    m_segment = vm_region;
    m_read_only = read_only;

    const auto file_name = priv_make_file_name(m_base_path);
    if (!mdtl::file_exist(file_name)) {
      std::cerr << "Segment file does not exist" << std::endl;
      return false;
    }

    // store = new Umap::SparseStore(file_name,read_only);
    m_segment_size = get_size(base_path);  // store->get_current_capacity();
    assert(m_segment_size % page_size() == 0);
    if (!priv_map_file_open(file_name, m_segment_size,
                            static_cast<char *>(m_segment),
                            read_only)) {  // , store)) {
      std::abort();                        // Fatal error
    }

    if (!read_only) {
      priv_test_file_space_free(base_path);
    }

    return true;
  }

  /// \brief This function does nothing in this implementation.
  /// \param new_segment_size Not used.
  /// \return Always returns true.
  bool extend(const size_type new_segment_size) {
    assert(priv_inited());

    if (m_read_only) {
      return false;
    }

    if (new_segment_size > m_vm_region_size) {
      std::cerr << "Requested segment size is bigger than the reserved VM size"
                << std::endl;
      return false;
    }
    if (new_segment_size > m_segment_size) {
      std::cerr << "Requested segment size is too big" << std::endl;
      return false;
    }

    return true;
  }

  /// \brief Destroy (unmap) the segment.
  void destroy() { priv_destroy_segment(); }

  /// \brief Syncs the segment (files) with the storage.
  /// \param sync Not used.
  void sync(const bool sync) { priv_sync_segment(sync); }

  /// \brief This function does nothing.
  /// \param offset Not used.
  /// \param nbytes Not used.
  void free_region(const different_type offset, const size_type nbytes) {
    priv_free_region(offset, nbytes);
  }

  /// \brief Returns the address of the segment.
  /// \return The address of the segment.
  void *get_segment() const { return m_segment; }

  /// \brief Returns the size of the segment.
  /// \return The size of the segment.
  size_type size() const { return m_segment_size; }

  /// \brief Returns the Umap page size.
  /// \return The current Umap page size.
  size_type page_size() const { return m_umap_page_size; }

  /// \brief Returns whether the segment is read only or not.
  /// \return Returns true if it is read only; otherwise, false.
  bool read_only() const { return m_read_only; }

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //

  // -------------------- //
  // Private methods (not designed to be used by the base class)
  // -------------------- //
  static std::string priv_make_file_name(const std::string &base_path) {
    return base_path + "_umap_sparse_segment_file";
  }

  void priv_reset() {
    m_umap_page_size = 0;
    m_vm_region_size = 0;
    m_segment_size = 0;
    m_segment = nullptr;
    // m_read_only = false;
  }

  bool priv_inited() const {
    return (m_umap_page_size > 0 && m_vm_region_size > 0 &&
            m_segment_size > 0 && m_segment && !m_base_path.empty());
  }

  bool priv_create_and_map_file(const std::string &base_path,
                                const size_type file_size,
                                void *const addr) const {
    assert(!m_segment ||
           static_cast<char *>(m_segment) + m_segment_size <= addr);

    const std::string file_name = priv_make_file_name(base_path);
    if (!priv_map_file_create(file_name, file_size, addr)) {
      return false;
    }
    return true;
  }

  size_t priv_get_sparsestore_file_granularity() const {
    char *file_granularity_str = getenv("SPARSE_STORE_FILE_GRANULARITY");
    size_t file_granularity;
    if (file_granularity_str == NULL) {
      file_granularity = SPARSE_STORE_FILE_GRANULARITY_DEFAULT;
    } else {
      file_granularity = (size_t)std::stol(file_granularity_str);
    }
    return file_granularity;
  }

  bool priv_map_file_create(const std::string &path, const size_type file_size,
                            void *const addr) const {
    assert(!path.empty());
    assert(file_size > 0);
    assert(addr);

    size_t sparse_file_granularity = priv_get_sparsestore_file_granularity();
    size_t page_size = umapcfg_get_umap_page_size();
    if (page_size == -1) {
      ::perror("umapcfg_get_umap_page_size failed");
      std::cerr << "errno: " << errno << std::endl;
    }

    store = new Umap::SparseStore(file_size, page_size, path,
                                  sparse_file_granularity);

    const int prot = PROT_READ | PROT_WRITE;
    const int flags = UMAP_PRIVATE | MAP_FIXED;
    void *const region =
        Umap::umap_ex(addr, file_size, prot, flags, -1, 0, store);
    if (region == UMAP_FAILED) {
      std::ostringstream ss;
      ss << "umap_mf of " << file_size << " bytes failed for " << path;
      perror(ss.str().c_str());
      return false;
    }

    return true;
  }

  bool priv_map_file_open(const std::string &path, const size_type file_size,
                          void *const addr, const bool read_only) {
    assert(!path.empty());
    assert(addr);

    // MEMO: one of the following options does not work on /tmp?

    size_t page_size = umapcfg_get_umap_page_size();
    if (page_size == -1) {
      ::perror("umapcfg_get_umap_page_size failed");
      std::cerr << "errno: " << errno << std::endl;
    }

    store = new Umap::SparseStore(path, read_only);
    uint64_t region_size = file_size;

    const int prot = PROT_READ | (read_only ? 0 : PROT_WRITE);
    const int flags = UMAP_PRIVATE | MAP_FIXED;
    void *const region =
        Umap::umap_ex(addr, region_size, prot, flags, -1, 0, store);
    if (region == UMAP_FAILED) {
      std::ostringstream ss;
      ss << "umap_mf of " << region_size << " bytes failed for " << path;
      perror(ss.str().c_str());
      return false;
    }

    return true;
  }

  void priv_unmap_file() {
    const auto file_name = priv_make_file_name(m_base_path);
    assert(mdtl::file_exist(file_name));

    if (::uunmap(static_cast<char *>(m_segment), m_segment_size) != 0) {
      std::cerr << "Failed to unmap a Umap region" << std::endl;
      std::abort();
    }

    m_segment_size = 0;
    int sparse_store_close_files = store->close_files();
    if (sparse_store_close_files != 0) {
      std::cerr << "Error closing SparseStore files" << std::endl;
      delete store;
      std::abort();
    }
    delete store;
  }

  void priv_destroy_segment() {
    if (!priv_inited()) return;

    priv_unmap_file();

    priv_reset();
  }

  void priv_sync_segment([[maybe_unused]] const bool sync) {
    if (!priv_inited() || m_read_only) return;

    if (::umap_flush() != 0) {
      std::cerr << "Failed umap_flush()" << std::endl;
    }
  }

  // MEMO: Umap cannot free file region
  bool priv_free_region(const different_type offset, const size_type nbytes) {
    if (!priv_inited() || m_read_only) return false;

    if (offset + nbytes > m_segment_size) return false;

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
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  ssize_t m_umap_page_size{0};
  size_type m_vm_region_size{0};
  size_type m_segment_size{0};
  void *m_segment{nullptr};
  std::string m_base_path;
  bool m_read_only;
  bool m_free_file_space{true};
  mutable Umap::SparseStore *store;
};

}  // namespace kernel
}  // namespace metall

#endif  // METALL_DETAILL_SEGMENT_STORAGE_UMAP_SPARSE_SEGMENT_STORAGE_HPP
