// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#pragma once

#include <string>
#include <filesystem>
#include <initializer_list>

#include <metall/detail/file.hpp>
#include <metall/detail/file_clone.hpp>

namespace metall::kernel {

namespace {
namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;
}  // namespace

/// \brief Manage directory structure of a datastore.
class storage {
 public:
  using path_type = std::filesystem::path;

  static path_type get_path(const path_type &base_path, const path_type &key) {
    return get_path(base_path, {key});
  }

  static path_type get_path(const path_type &base_path,
                            const std::initializer_list<path_type> &paths) {
    auto path = priv_get_root_path(base_path);
    for (const auto &p : paths) {
      path /= p;
    }
    return path;
  }

  /// \brief Create a new datastore. If a datastore already exists, remove it
  /// and create a new one.
  /// \param base_path A path to a directory where a datastore is created.
  /// \return On success, returns true. On error, returns false.
  static bool create(const path_type &base_path) {
    const auto root_dir = priv_get_root_path(base_path);

    // Remove existing directory to certainly create a new data store
    if (!mdtl::remove_file(root_dir)) {
      std::string s("Failed to remove a directory: " + root_dir.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    if (!mdtl::create_directory(root_dir)) {
      std::string s("Failed to create directory: " + root_dir.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }

    return true;
  }

  /// \brief Remove a datastore.
  /// \param base_path A path to an existing datastore.
  /// \return On success, returns true. On error, returns false.
  static bool remove(const path_type &base_path) {
    const auto root_dir = priv_get_root_path(base_path);
    if (!mdtl::remove_file(root_dir)) {
      std::string s("Failed to remove a directory: " + root_dir.string());
      logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
      return false;
    }
    return true;
  }

 private:
  static path_type priv_get_root_path(const path_type &base_path) {
    return base_path / "mds";
  }
};

}  // namespace metall::kernel