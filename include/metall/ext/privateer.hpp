// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#pragma once

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>

#include <string>
#include <iostream>
#include <cassert>
#include <filesystem>
#include <thread>
#include <mutex>
#include <sstream>

#include <privateer/privateer.hpp>

#include <metall/basic_manager.hpp>
#include <metall/detail/file.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/defs.hpp>
#include <metall/logger.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/storage.hpp>

namespace metall {

namespace {
namespace mdtl = metall::mtlldetail;
}

class privateer_storage;
class privateer_segment_storage;

/// \brief Metall manager with Privateer.
using manager_privateer =
    basic_manager<privateer_storage, privateer_segment_storage>;

#ifdef METALL_USE_PRIVATEER
using manager = manager_privateer;
#endif

class privateer_storage : public metall::kernel::storage {
 public:
  using path_type = std::filesystem::path;

  static path_type get_path(const path_type &raw_path, const path_type &key) {
    return get_path(raw_path, {key});
  }

  static path_type get_path(const path_type &raw_path,
                            const std::initializer_list<path_type> &subpaths) {
    auto root_path = priv_get_root_path(raw_path.string());
    for (const auto &p : subpaths) {
      root_path /= p;
    }
    return root_path;
  }

 private:
  static path_type priv_get_root_path(const std::string &raw_path) {
    std::string base_dir = "";
    size_t stash_path_index = raw_path.find("<stash>");
    if (stash_path_index != std::string::npos) {
      base_dir = raw_path.substr((stash_path_index + 7),
                                 raw_path.length() - (stash_path_index + 7));
    } else {
      base_dir = raw_path;
    }
    return base_dir;
  }
};

class privateer_segment_storage {
 public:
  using path_type = privateer_storage::path_type;
  using segment_header_type = metall::kernel::segment_header;

  privateer_segment_storage() { priv_load_system_page_size(); }

  ~privateer_segment_storage() {
    priv_sync_segment(true);
    release();
  }

  privateer_segment_storage(const privateer_segment_storage &) = delete;
  privateer_segment_storage &operator=(const privateer_segment_storage &) =
      delete;

  privateer_segment_storage(privateer_segment_storage &&other) noexcept
      : m_system_page_size(other.m_system_page_size),
        m_vm_region_size(other.m_vm_region_size),
        m_current_segment_size(other.m_current_segment_size),
        m_vm_region(other.m_vm_region),
        m_segment(other.m_segment),
        m_segment_header(other.m_segment_header),
        m_base_path(other.m_base_path),
        m_read_only(other.m_read_only),
        m_privateer(other.m_privateer),
        m_privateer_version_name(other.m_privateer_version_name) {
    other.priv_reset();
  }

  privateer_segment_storage &operator=(
      privateer_segment_storage &&other) noexcept {
    m_system_page_size = std::move(other.m_system_page_size);
    m_vm_region_size = std::move(other.m_vm_region_size);
    m_current_segment_size = std::move(other.m_current_segment_size);
    m_vm_region = std::move(other.m_vm_region);
    m_segment = std::move(other.m_segment);
    m_segment_header = std::move(other.m_segment_header);
    m_base_path = std::move(other.m_base_path);
    m_read_only = std::move(other.m_read_only);
    m_privateer = std::move(other.m_privateer);
    m_privateer_version_name = std::move(other.m_privateer_version_name);

    other.priv_reset();

    return (*this);
  }

  static bool copy(const std::string &source_path,
                   const std::string &destination_path,
                   [[maybe_unused]] const bool clone,
                   const int max_num_threads) {
    if (!mtlldetail::copy_files_in_directory_in_parallel(
            parse_path(source_path).first, parse_path(destination_path).first,
            max_num_threads)) {
      return false;
    }

    return true;
  }

  bool snapshot(std::string destination_path, const bool clone,
                const int max_num_threads) {
    sync(true);
    auto path = parse_path(destination_path).first;
    std::pair<std::string, std::string> parsed_path = priv_parse_path(path);
    std::string version_path = parsed_path.second;
    if (!m_privateer->snapshot(version_path.c_str())) {
      return false;
    }
    if (!copy(m_base_path, path, clone, max_num_threads)) {
      return false;
    }
    return true;
  }

  bool create(const path_type &base_path, const std::size_t capacity) {
    assert(!priv_inited());
    init_privateer_datastore(base_path.string(), Privateer::CREATE);
    m_base_path = parse_path(base_path).first;
    const auto header_size = priv_aligned_header_size();
    const auto vm_region_size = header_size + capacity;
    if (!priv_reserve_vm(vm_region_size)) {
      return false;
    }
    m_segment = reinterpret_cast<char *>(m_vm_region) + header_size;
    priv_construct_segment_header(m_vm_region);
    m_read_only = false;

    const auto segment_size = vm_region_size - header_size;

    if (!priv_create_and_map_file(m_base_path, segment_size, m_segment)) {
      priv_reset();
      return false;
    }
    return true;
  }

  bool open(const std::string &base_path, const std::size_t,
            const bool read_only) {
    assert(!priv_inited());
    init_privateer_datastore(base_path, Privateer::OPEN);

    m_base_path = parse_path(base_path).first;
    m_read_only = read_only;

    const auto header_size = priv_aligned_header_size();
    const auto segment_size = Privateer::version_capacity(m_base_path.c_str());
    const auto vm_size = header_size + segment_size;
    if (!priv_reserve_vm(vm_size)) {
      return false;
    }
    m_segment = reinterpret_cast<char *>(m_vm_region) + header_size;
    priv_construct_segment_header(m_vm_region);

    if (!priv_map_file_open(m_base_path, static_cast<char *>(m_segment),
                            read_only)) {  // , store)) {
      std::abort();                        // Fatal error
    }

    return true;
  }

  bool extend(const std::size_t) {
    // TODO: check errors
    return true;
  }

  void init_privateer_datastore(std::string path, int action) {
    const std::lock_guard<std::mutex> lock(m_create_mutex);
    std::pair<std::string, std::string> base_stash_pair = parse_path(path);
    std::string base_dir = base_stash_pair.first;
    std::string stash_dir = base_stash_pair.second;
    std::pair<std::string, std::string> parsed_path = priv_parse_path(base_dir);
    std::string privateer_base_path = parsed_path.first;
    std::string version_path = parsed_path.second;
    m_privateer_version_name = version_path;
    /* int action =
        std::filesystem::exists(std::filesystem::path(privateer_base_path))
            ? Privateer::OPEN
            : Privateer::CREATE; */
    if (!stash_dir.empty()) {
      m_privateer =
          new Privateer(action, privateer_base_path.c_str(), stash_dir.c_str());
    } else {
      m_privateer = new Privateer(action, privateer_base_path.c_str());
    }
    if (action == Privateer::CREATE) {
      m_privateer_block_size = m_privateer->get_block_size();
    } else {
      std::string version_full_path = privateer_base_path + "/" + version_path;
      m_privateer_block_size = Privateer::version_block_size(version_full_path);
    }
  }

  static std::pair<std::string, std::string> parse_path(std::string path) {
    std::string base_dir = "";
    std::string stash_dir = "";
    size_t stash_path_index = path.find("<stash>");
    if (stash_path_index != std::string::npos) {
      stash_dir = path.substr(0, stash_path_index);
      base_dir = path.substr((stash_path_index + 7),
                             path.length() - (stash_path_index + 7));
    } else {
      base_dir = path;
    }
    return std::pair<std::string, std::string>(base_dir, stash_dir);
  }

  void release() { priv_release(); }

  void sync(const bool sync) { priv_sync_segment(sync); }

  void free_region(const std::ptrdiff_t, const std::size_t) {
    // Do nothing
    // Privateer does not free file region
  }

  void *get_segment() const { return m_segment; }

  segment_header_type &get_segment_header() {
    return *reinterpret_cast<segment_header_type *>(m_vm_region);
  }

  const segment_header_type &get_segment_header() const {
    return *reinterpret_cast<const segment_header_type *>(m_vm_region);
  }

  std::size_t size() const { return m_current_segment_size; }

  std::size_t page_size() const { return m_system_page_size; }

  bool read_only() const { return m_read_only; }

  bool is_open() const { return !!m_privateer; }

  bool check_sanity() const {
    // TODO: implement
    return true;
  }

 private:
  std::size_t priv_aligment() const {
    // FIXME
    // return 1 << 28;
    if (m_system_page_size < m_privateer_block_size) {
      return mdtl::round_up(int64_t(m_privateer_block_size),
                            int64_t(m_system_page_size));
    } else {
      return mdtl::round_up(int64_t(m_system_page_size),
                            int64_t(m_privateer_block_size));
    }
  }

  void priv_reset() {
    m_vm_region_size = 0;
    m_current_segment_size = 0;
    m_vm_region = nullptr;
    m_segment = nullptr;
    m_segment_header = nullptr;
    m_base_path.clear();
    m_privateer = nullptr;
    m_privateer_version_name.clear();
  }

  bool priv_inited() const {
    if (m_privateer) {
      assert(m_system_page_size > 0 && m_vm_region_size > 0 &&
             m_current_segment_size > 0 && m_segment && !m_base_path.empty());
      return true;
    }
    return false;
  }

  bool priv_create_and_map_file(const std::string &file_name,
                                const std::size_t file_size, void *const addr) {
    assert(!m_segment ||
           static_cast<char *>(m_segment) + m_current_segment_size <= addr);

    if (!priv_map_file_create(file_name, file_size, addr)) {
      return false;
    }
    return true;
  }

  bool priv_map_file_create(const std::string &path,
                            const std::size_t file_size, void *const addr) {
    assert(!path.empty());
    assert(file_size > 0);
    assert(addr);
    void *data = m_privateer->create(addr, m_privateer_version_name.c_str(),
                                     file_size, true);
    if (data == nullptr) {
      return false;
    }

    m_current_segment_size = file_size;  // privateer->current_size();
    return true;
  }

  bool priv_map_file_open(const std::string &path, void *const addr,
                          const bool read_only) {
    assert(!path.empty());
    assert(addr);

    void *data =
        read_only ? m_privateer->open_read_only(
                        addr, m_privateer_version_name.c_str())
                  : m_privateer->open(addr, m_privateer_version_name.c_str());
    m_current_segment_size = m_privateer->region_size();
    return true;
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

  void priv_release() {
    if (!priv_inited()) return;

    delete m_privateer;
    m_privateer = nullptr;

    // another therad gets the vm region

    // Just erase segment header
    mdtl::map_with_prot_none(m_vm_region, m_vm_region_size);
    mdtl::munmap(m_vm_region, m_vm_region_size, false);

    priv_reset();
  }

  std::size_t priv_aligned_header_size() {
    const auto size =
        mdtl::round_up(sizeof(segment_header_type), int64_t(priv_aligment()));
    return size;
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

  void priv_sync_segment(const bool) {
    if (!priv_inited() || m_read_only) return;
    m_privateer->msync();
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

  std::pair<std::string, std::string> priv_parse_path(std::string path) {
    std::pair<std::string, std::string> parsed;
    size_t position = 0;
    std::string token = "/";
    position = path.find_last_of(token);
    std::string privateer_base_path = path.substr(0, position);
    std::string version_name = path.substr(position + 1, path.length());
    parsed = std::make_pair(privateer_base_path, version_name);
    return parsed;
  }

  ssize_t m_system_page_size{0};
  std::size_t m_privateer_block_size{0};
  std::size_t m_vm_region_size{0};
  std::size_t m_current_segment_size{0};
  void *m_vm_region{nullptr};
  void *m_segment{nullptr};
  segment_header_type *m_segment_header{nullptr};
  std::string m_base_path{};
  bool m_read_only{false};
  mutable Privateer *m_privateer{nullptr};
  std::string m_privateer_version_name{};
  std::mutex m_create_mutex{};
};

}  // namespace metall
