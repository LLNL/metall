// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_PTREE_HPP
#define METALL_DETAIL_UTILITY_PTREE_HPP

#include <string>
#include <filesystem>

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

#include <metall/logger.hpp>

namespace metall::mtlldetail::ptree {

namespace {
namespace fs = std::filesystem;
namespace bptree = boost::property_tree;
}

using node_type = bptree::ptree;

inline bool validate_key(const std::string &key) {
  using path_type = bptree::ptree::path_type;
  return path_type(key).single();  // To avoid confusion, a multi-layer path
                                   // style string is invalid.
}

inline bool empty(const node_type &tree) { return tree.empty(); }

inline std::size_t count(const node_type &tree, const std::string &key) {
  if (!validate_key(key)) {
    std::string s("Invalid key: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return 0;
  }
  return tree.count(key);
}

template <typename value_type>
inline bool get_value(const node_type &tree, const std::string &key,
                      value_type *const value) {
  if (!validate_key(key)) {
    std::string s("Invalid key: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  try {
    *value = tree.get<value_type>(key);
  } catch (const bptree::ptree_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    return false;
  }
  return true;
}

inline bool get_child(const node_type &tree, const std::string &key,
                      node_type *out) {
  if (!validate_key(key)) {
    std::string s("Invalid key: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  if (auto child = tree.get_child_optional(key)) {
    *out = *child;
    return true;
  }
  return false;
}

template <typename value_type>
inline bool add_value(const std::string &key, const value_type &value,
                      node_type *tree) {
  if (!validate_key(key)) {
    std::string s("Invalid key: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  try {
    tree->add(key, value);
  } catch (...) {
    std::string s("Failed to add: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }
  return true;
}

inline bool add_child(const std::string &key, const node_type &child,
                      node_type *tree) {
  if (!validate_key(key)) {
    std::string s("Invalid key: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }

  try {
    tree->add_child(key, child);
  } catch (...) {
    std::string s("Failed to add: " + key);
    logger::out(logger::level::error, __FILE__, __LINE__, s.c_str());
    return false;
  }
  return true;
}

inline bool push_back(const node_type &child, node_type *parent) {
  try {
    parent->push_back(std::make_pair("", child));
  } catch (...) {
    logger::out(logger::level::error, __FILE__, __LINE__,
                "Failed to pushback an item");
    return false;
  }
  return true;
}

inline bool read_json(const fs::path &file_name, node_type *root) {
  try {
    bptree::read_json(file_name, *root);
  } catch (const bptree::json_parser_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    return false;
  }
  return true;
}

inline bool write_json(const node_type &root, const fs::path &file_name) {
  try {
    bptree::write_json(file_name, root);
  } catch (const bptree::json_parser_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    return false;
  }
  return true;
}

inline bool serialize(const node_type &root, std::string *const out_str) {
  try {
    bptree::write_json(*out_str, root);
  } catch (const bptree::json_parser_error &e) {
    logger::out(logger::level::error, __FILE__, __LINE__, e.what());
    return false;
  }
  return true;
}

inline std::size_t erase(const std::string &key, node_type *tree) {
  return tree->erase(key);
}

}  // namespace metall::mtlldetail::ptree

#endif  // METALL_DETAIL_UTILITY_PTREE_HPP
