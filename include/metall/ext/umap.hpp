// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#pragma once

#include <string>

#include <umap/store/SparseStore.h>

#include <metall/defs.hpp>
#include <metall/logger.hpp>
#include <metall/basic_manager.hpp>
#include <metall/kernel/segment_header.hpp>
#include <metall/kernel/storage.hpp>

namespace metall {

namespace {
namespace mdtl = metall::mtlldetail;
}

class umap_storage;
class umap_segment_storage;

/// \brief Metall manager with Privateer.
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

  umap_segment_storage() {}

  ~umap_segment_storage() {}

  umap_segment_storage(const umap_segment_storage &) = delete;
  umap_segment_storage &operator=(const umap_segment_storage &) = delete;

  umap_segment_storage(umap_segment_storage &&other) noexcept {}
  umap_segment_storage &operator=(umap_segment_storage &&other) noexcept {}

  static bool copy(const path_type &source_path,
                   const path_type &destination_path,
                   [[maybe_unused]] const bool clone,
                   const int max_num_threads);

  bool snapshot(path_type destination_path, const bool clone,
                const int max_num_threads);

  bool create(const path_type &base_path, const std::size_t capacity);

  bool open(const path_type &base_path, const std::size_t,
            const bool read_only);

  bool extend(const std::size_t);

  void release();

  void sync(const bool sync);

  void free_region(const std::ptrdiff_t, const std::size_t);

  void *get_segment() const;

  segment_header_type &get_segment_header();

  const segment_header_type &get_segment_header() const;

  std::size_t size() const;

  std::size_t page_size() const;

  bool read_only() const;

  bool is_open() const;

  bool check_sanity() const;
};

}  // namespace metall