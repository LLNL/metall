// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_DATASTORE_LS_HPP
#define METALL_UTILITY_DATASTORE_LS_HPP

#include <iostream>
#include <cstdlib>
#include <string>
#include <vector>
#include <iomanip>
#include <numeric>

#include <metall/metall.hpp>

namespace metall::utility {

#ifndef DOXYGEN_SKIP
namespace datastore_ls_detail {
inline void aligned_show(const std::vector<std::vector<std::string>> &buf) {
  if (buf.empty()) return;

  // Calculate each column size
  std::vector<std::size_t> col_size(buf.front().size(), 0);
  for (const auto &row : buf) {
    assert(col_size.size() == row.size());
    for (std::size_t c = 0; c < row.size(); ++c) {
      col_size[c] = std::max(row[c].size(), col_size[c]);
    }
  }

  // Show column title
  {
    std::cout << "|";
    for (std::size_t c = 0; c < col_size.size(); ++c) {
      std::cout << std::setw(col_size[c] + 2) << buf[0][c] << " |";
    }
    std::cout << std::endl;

    // Show horizontal rule
    for (std::size_t c = 0; c < col_size.size(); ++c) {
      for (std::size_t i = 0; i < col_size[c] + 4; ++i) {
        std::cout << "-";
      }
    }
    std::cout << std::endl;
  }

  // Show items
  for (std::size_t l = 1; l < buf.size(); ++l) {
    const auto &row = buf[l];

    std::cout << "|";
    for (std::size_t c = 0; c < col_size.size(); ++c) {
      std::cout << std::setw(col_size[c] + 2) << std::right << row[c] << " |";
    }
    std::cout << std::endl;
  }
}
}  // namespace datastore_ls_detail
#endif  // DOXYGEN_SKIP

inline void ls_named_object(const std::string &datastore_path) {
  std::cout << "[Named Object]" << std::endl;
  auto accessor =
      metall::manager::access_named_object_attribute(datastore_path.c_str());
  if (!accessor.good()) {
    std::cerr << "Failed to open datastore" << std::endl;
    std::abort();
  }

  std::vector<std::vector<std::string>> buf;
  buf.emplace_back(std::vector<std::string>{"Name", "Length", "Offset",
                                            "Type-ID", "Description"});
  for (const auto &object : accessor) {
    std::vector<std::string> row;
    row.push_back(object.name());
    row.push_back(std::to_string(object.length()));
    row.push_back(std::to_string(object.offset()));
    row.push_back(std::to_string(object.type_id()));
    row.push_back(object.description());
    buf.emplace_back(std::move(row));
  }
  datastore_ls_detail::aligned_show(buf);
}

inline void ls_unique_object(const std::string &datastore_path) {
  std::cout << "[Unique Object]" << std::endl;
  auto accessor =
      metall::manager::access_unique_object_attribute(datastore_path.c_str());
  if (!accessor.good()) {
    std::cerr << "Failed to open datastore" << std::endl;
    std::abort();
  }

  std::vector<std::vector<std::string>> buf;
  buf.emplace_back(std::vector<std::string>{
      "Name: typeid(T).name()", "Length", "Offset", "Type-ID", "Description"});
  for (const auto &object : accessor) {
    std::vector<std::string> row;
    row.push_back(object.name());
    row.push_back(std::to_string(object.length()));
    row.push_back(std::to_string(object.offset()));
    row.push_back(std::to_string(object.type_id()));
    row.push_back(object.description());
    buf.emplace_back(std::move(row));
  }
  datastore_ls_detail::aligned_show(buf);
}

inline void ls_anonymous_object(const std::string &datastore_path) {
  std::cout << "[Anonymous Object]" << std::endl;
  auto accessor = metall::manager::access_anonymous_object_attribute(
      datastore_path.c_str());
  if (!accessor.good()) {
    std::cerr << "Failed to open datastore" << std::endl;
    std::abort();
  }

  std::vector<std::vector<std::string>> buf;
  buf.emplace_back(
      std::vector<std::string>{"Length", "Offset", "Type-ID", "Description"});
  for (const auto &object : accessor) {
    std::vector<std::string> row;
    row.push_back(std::to_string(object.length()));
    row.push_back(std::to_string(object.offset()));
    row.push_back(std::to_string(object.type_id()));
    row.push_back(object.description());
    buf.emplace_back(std::move(row));
  }
  datastore_ls_detail::aligned_show(buf);
}

}  // namespace metall::utility

#endif  // METALL_UTILITY_DATASTORE_LS_HPP
