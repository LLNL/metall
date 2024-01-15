// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#pragma once

#include <string>

#include <umap/umap.h>
#include <umap/store/SparseStore.h>

#include <metall/defs.hpp>
#include <metall/logger.hpp>
#include <metall/basic_manager.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/storage.hpp>

// UMap SparseStore file granularity
const size_t SPARSE_STORE_FILE_GRANULARITY_DEFAULT = 8388608L;

namespace metall {

namespace {
namespace mdtl = metall::mtlldetail;
}

class umap_storage;
class umap_segment_storage;

/// \brief Metall manager with UMap SparseStore.
using manager_umap = basic_manager<umap_storage, umap_segment_storage>;

#ifdef METALL_USE_UMAP
using manager = manager_umap;
#endif

class umap_storage : public metall::kernel::storage {
 private:
  using base_type = metall::kernel::storage;

 public:
  using base_type::base_type;
};

class umap_segment_storage {
 public:
  using path_type = umap_storage::path_type;
  using segment_header_type = metall::kernel::segment_header;

  umap_segment_storage() {
    m_umap_page_size = ::umapcfg_get_umap_page_size();
    if (m_umap_page_size == -1) {
      std::cerr << "Failed to get Umap pagesize" << std::endl;
      std::abort();
    }
    if (!priv_load_system_page_size()) {
      std::abort();
    }
  }

  ~umap_segment_storage() {
    priv_sync_segment(true);
    release();
  }

  umap_segment_storage(const umap_segment_storage &) = delete;
  umap_segment_storage &operator=(const umap_segment_storage &) = delete;

  umap_segment_storage(umap_segment_storage &&other) noexcept {}
  umap_segment_storage &operator=(umap_segment_storage &&other) noexcept {}

  static bool copy(const path_type &source_path,
                   const path_type &destination_path,
                   [[maybe_unused]] const bool clone,
                   const int max_num_threads) {
    /*if (clone) {
      std::string s("Clone: " + source_path.u8string());
      logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
      return mdtl::clone_file(source_path, destination_path);
    } */ /* else { */

    /* Create SparseStore Metadata dir */
    const auto sparse_store_source_path =
        priv_make_file_name(source_path.u8string());
    const auto sparse_store_dst_path =
        priv_make_file_name(destination_path.u8string());

    if (!mdtl::create_directory(sparse_store_dst_path)) {
      std::stringstream ss;
      ss << "Failed to create directory: " << sparse_store_dst_path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }
    std::string s("Copy: " + source_path.u8string());
    logger::out(logger::level::info, __FILE__, __LINE__, s.c_str());
    if (!mdtl::copy_files_in_directory_in_parallel(
            source_path, destination_path, max_num_threads)) {
      return false;
    }

    if (!mdtl::copy_files_in_directory_in_parallel(
            sparse_store_source_path, sparse_store_dst_path, max_num_threads)) {
      return false;
    }

    return true;
    /* } */
    /* assert(false);
    return false; */
  }

  bool snapshot(path_type destination_path, const bool clone,
                const int max_num_threads) {
    sync(true);
    if (!copy(m_base_path, destination_path, clone, max_num_threads)) {
      return false;
    }
    return true;
  }

  bool create(const path_type &base_path, const std::size_t capacity) {
    assert(!priv_inited());

    m_base_path = base_path;

    const auto header_size = priv_aligned_header_size();
    const auto vm_region_size = header_size + capacity;
    if (!priv_reserve_vm(vm_region_size)) {
      return false;
    }
    m_segment = reinterpret_cast<char *>(m_vm_region) + header_size;
    m_current_segment_size = vm_region_size - header_size;
    priv_construct_segment_header(m_vm_region);
    m_read_only = false;

    const auto segment_size = vm_region_size - header_size;

    // assert(!path.empty());
    // assert(file_size > 0);
    // assert(addr);

    size_t sparse_file_granularity = priv_get_sparsestore_file_granularity();
    size_t page_size = umapcfg_get_umap_page_size();
    if (page_size == -1) {
      ::perror("umapcfg_get_umap_page_size failed");
      std::cerr << "errno: " << errno << std::endl;
      return false;
    }
    const std::string file_name = priv_make_file_name(base_path);
    /* store = new Umap::SparseStore(segment_size, page_size, file_name,
                                  page_size); */
    store = std::make_unique<Umap::SparseStore>(segment_size, page_size,
                                                file_name, page_size);

    const int prot = PROT_READ | PROT_WRITE;
    const int flags = UMAP_PRIVATE | UMAP_FIXED;
    m_segment = Umap::umap_ex(m_segment, (segment_size - m_umap_page_size),
                              prot, flags, -1, 0, store.get());
    if (m_segment == UMAP_FAILED) {
      std::ostringstream ss;
      ss << "umap_mf of " << segment_size << " bytes failed for "
         << m_base_path;
      perror(ss.str().c_str());
      return false;
    }
    return true;
  }

  bool open(const path_type &base_path, const std::size_t,
            const bool read_only) {
    assert(!priv_inited());
    m_base_path = base_path;

    const auto header_size = priv_aligned_header_size();
    const auto directory_name = priv_make_file_name(base_path);
    m_current_segment_size = Umap::SparseStore::get_capacity(directory_name);
    m_vm_region_size = header_size + m_current_segment_size;
    if (!priv_reserve_vm(m_vm_region_size)) {
      return false;
    }
    m_segment = reinterpret_cast<char *>(m_vm_region) + header_size;
    priv_construct_segment_header(m_vm_region);

    m_read_only = read_only;

    size_t page_size = umapcfg_get_umap_page_size();
    if (page_size == -1) {
      ::perror("umapcfg_get_umap_page_size failed");
      std::cerr << "errno: " << errno << std::endl;
    }

    // store = new Umap::SparseStore(directory_name, read_only);
    store = std::make_unique<Umap::SparseStore>(directory_name, read_only);
    const int prot = PROT_READ | (read_only ? 0 : PROT_WRITE);
    const int flags = UMAP_PRIVATE | UMAP_FIXED;
    m_segment =
        Umap::umap_ex(m_segment, (m_current_segment_size - m_umap_page_size),
                      prot, flags, -1, 0, store.get());
    if (m_segment == UMAP_FAILED) {
      std::ostringstream ss;
      ss << "umap_mf of " << m_current_segment_size << " bytes failed for "
         << directory_name;
      perror(ss.str().c_str());
      return false;
    }
    return true;
  }

  bool extend(const std::size_t) { /* TODO implement */ return true; }

  void release() { priv_release(); }

  void sync(const bool sync) { priv_sync_segment(sync); }

  void free_region(const std::ptrdiff_t, const std::size_t) {
    /* not currently supported in Umap?? */
  }

  void *get_segment() const { return m_segment; }

  segment_header_type &get_segment_header() {
    return *reinterpret_cast<segment_header_type *>(m_vm_region);
  }

  const segment_header_type &get_segment_header() const {
    return *reinterpret_cast<const segment_header_type *>(m_vm_region);
  }

  std::size_t size() const { return m_current_segment_size; }

  std::size_t page_size() const {
    return m_system_page_size;  // m_umap_page_size;
  }

  bool read_only() const { return m_read_only; }

  bool is_open() const { return !!store; }

  bool check_sanity() const { /* TODO implement */ return true; }

 private:
  bool priv_inited() const {
    return (m_umap_page_size > 0 && m_vm_region_size > 0 &&
            m_current_segment_size > 0 && m_segment && !m_base_path.empty());
  }

  std::size_t priv_aligned_header_size() {
    const auto size =
        mdtl::round_up(sizeof(segment_header_type), int64_t(priv_aligment()));
    return size;
  }

  std::size_t priv_aligment() const {
    // FIXME
    // return 1 << 28;
    size_t result;
    if (m_system_page_size < m_umap_page_size) {
      /* return */ result = mdtl::round_up(int64_t(m_umap_page_size),
                                           int64_t(m_system_page_size));
    } else {
      /* return */ result = mdtl::round_up(int64_t(m_system_page_size),
                                           int64_t(m_umap_page_size));
    }
    return result;
  }

  bool priv_construct_segment_header(void *const addr) {
    if (!addr) {
      return false;
    }

    const auto size = priv_aligned_header_size();
    if (mdtl::map_anonymous_write_mode(addr, size, MAP_FIXED) != addr) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Cannot allocate segment header");
      return false;
    }
    m_segment_header = reinterpret_cast<segment_header_type *>(addr);

    new (m_segment_header) segment_header_type();

    return true;
  }

  static std::string priv_make_file_name(const std::string &base_path) {
    return base_path + "/umap_sparse_segment_file";
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

  bool priv_reserve_vm(const std::size_t nbytes) {
    m_vm_region_size =
        mdtl::round_up((int64_t)nbytes, (int64_t)priv_aligment());
    m_vm_region =
        mdtl::reserve_aligned_vm_region(priv_aligment(), m_vm_region_size);

    if (!m_vm_region) {
      std::stringstream ss;
      ss << "Cannot reserve a VM region " << nbytes << " bytes";
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      m_vm_region_size = 0;
      return false;
    }
    assert(reinterpret_cast<uint64_t>(m_vm_region) % priv_aligment() == 0);

    return true;
  }

  void priv_sync_segment(const bool) {
    if (!priv_inited() || m_read_only) return;
    if (::umap_flush() != 0) {
      std::cerr << "Failed umap_flush()" << std::endl;
      std::abort();
    }
  }

  void priv_release() {
    if (!priv_inited()) return;

    const auto file_name = priv_make_file_name(m_base_path);
    assert(mdtl::file_exist(file_name));
    if (::uunmap(static_cast<char *>(m_segment), m_current_segment_size) != 0) {
      std::cerr << "Failed to unmap a Umap region" << std::endl;
      std::abort();
    }
    m_current_segment_size = 0;
    int sparse_store_close_files =
        store.get()->close_files();  // store->close_files();
    if (sparse_store_close_files != 0) {
      std::cerr << "Error closing SparseStore files" << std::endl;
      std::abort();
    }
    mdtl::map_with_prot_none(m_vm_region, m_vm_region_size);
    mdtl::munmap(m_vm_region, m_vm_region_size, false);

    priv_reset();
  }

  void priv_reset() {
    m_umap_page_size = 0;
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_segment = nullptr;
    // m_read_only = false;
  }

  bool priv_load_system_page_size() {
    m_system_page_size = mdtl::get_page_size();
    if (m_system_page_size == -1) {
      logger::out(logger::level::critical, __FILE__, __LINE__,
                  "Failed to get system pagesize");
      return false;
    }
    return true;
  }

  size_t m_system_page_size{0};
  size_t m_umap_page_size{0};
  size_t m_current_segment_size{0};
  size_t m_vm_region_size{0};
  void *m_segment{nullptr};
  segment_header_type *m_segment_header{nullptr};
  std::string m_base_path{};
  void *m_vm_region{nullptr};
  bool m_read_only;
  // mutable Umap::SparseStore *store;
  std::unique_ptr<Umap::SparseStore> store{nullptr};
};

}  // namespace metall
