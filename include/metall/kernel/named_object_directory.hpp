// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_NAMED_OBJECT_DIRECTORY_HPP
#define METALL_DETAIL_NAMED_OBJECT_DIRECTORY_HPP

#include <iostream>
#include <utility>
#include <string>
#include <fstream>
#include <cassert>
#include <functional>
#include <tuple>
#include <sstream>

#include <boost/container/string.hpp>
#include <boost/unordered_map.hpp>
#include <boost/unordered_set.hpp>

#include <metall/logger.hpp>
#include <metall/detail/utility/ptree.hpp>
#include <metall/detail/utility/hash.hpp>

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
namespace json = metall::detail::utility::ptree;
}

/// \brief Directory for named objects.
/// \tparam _offset_type
/// \tparam _size_type
/// \tparam _allocator_type
template <typename _offset_type, typename _size_type, typename _allocator_type = std::allocator<std::byte>>
class named_object_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using size_type = _size_type;
  using name_type = std::string;
  using offset_type = _offset_type;
  using length_type = size_type;
  using description_type = std::string;
  using allocator_type = _allocator_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using key_string_type = name_type;

  using main_entry_type = std::tuple<name_type, offset_type, length_type, description_type>;
  using main_table_type = boost::unordered_map<key_string_type, main_entry_type, util::string_hash<key_string_type>>;

  using key_table_type = boost::unordered_set<key_string_type, util::string_hash<key_string_type>>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using key_iterator = typename key_table_type::const_iterator;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit named_object_directory([[maybe_unused]] const allocator_type &allocator = allocator_type())
      : m_main_table(),
        m_key_table() {}
  ~named_object_directory() noexcept = default;
  named_object_directory(const named_object_directory &) = default;
  named_object_directory(named_object_directory &&) noexcept = default;
  named_object_directory &operator=(const named_object_directory &) = default;
  named_object_directory &operator=(named_object_directory &&) noexcept = default;

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  /// \brief
  /// \param name
  /// \param offset
  /// \param length
  /// \param description
  /// \return
  bool insert(const name_type &name,
              const offset_type offset,
              const length_type length,
              const description_type &description = std::string()) {
    bool inserted = false;

    try {
      inserted = m_main_table.emplace(name, std::make_tuple(name, offset, length, description)).second;
      if (inserted != m_key_table.insert(name).second) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "Error occurred in an internal table");
        return false;
      }
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown");
      return false;
    }

    return inserted;
  }

  /// \brief
  /// \param name
  /// \return
  size_type count(const name_type &name) const {
    assert(m_main_table.count(name) == m_key_table.count(name));
    return m_main_table.count(name);
  }

  /// \brief
  /// \return
  key_iterator keys_begin() const {
    return m_key_table.cbegin();
  }

  /// \brief
  /// \return
  key_iterator keys_end() const {
    return m_key_table.cend();
  }

  /// \brief
  /// \param name
  /// \param buf
  /// \return
  bool get_offset(const name_type &name, offset_type *buf) const {
    auto itr = m_main_table.find(name);
    if (itr == m_main_table.end()) return false;

    *buf = std::get<index::offset>(itr->second);
    return true;
  }

  /// \brief
  /// \param name
  /// \param buf
  /// \return
  bool get_length(const name_type &name, length_type *buf) const {
    auto itr = m_main_table.find(name);
    if (itr == m_main_table.end()) return false;

    *buf = std::get<index::length>(itr->second);
    return true;
  }

  /// \brief
  /// \param name
  /// \param buf
  /// \return
  bool get_description(const name_type &name, description_type *buf) const {
    auto itr = m_main_table.find(name);
    if (itr == m_main_table.end()) return false;

    *buf = std::get<index::description>(itr->second);
    return true;
  }

  /// \brief
  /// \param name
  /// \param description
  /// \return
  bool set_description(const name_type &name, const description_type &description) {
    auto itr = m_main_table.find(name);
    if (itr == m_main_table.end()) return false;

    std::get<index::description>(itr->second) = description;
    return true;
  }

  /// \brief
  /// \param name
  /// \return
  size_type erase(const name_type &name) {
    size_type num_erased = 0;
    try {
      num_erased = m_key_table.erase(name);
      if (m_main_table.erase(name) != num_erased) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "Error occurred in an internal table");
        num_erased = 0;
      }
    } catch (...) {
      return 0;
    }
    return num_erased;
  }

  /// \brief
  /// \param path
  bool serialize(const char *const path) const {

    json::node_type json_named_objects_list;
    for (const auto &item : m_main_table) {
      json::node_type json_named_object_entry;
      if (!json::add_value(json_key::name, std::get<index::name>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::offset, std::get<index::offset>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::length, std::get<index::length>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::description,
                           std::get<index::description>(item.second),
                           &json_named_object_entry)) {
        return false;
      }
      if (!json::push_back(json_named_object_entry, &json_named_objects_list)) {
        return false;
      }
    }

    json::node_type json_root;
    if (!json::add_child(json_key::named_objects, json_named_objects_list, &json_root)) {
      return false;
    }

    if (!json::write_json(json_root, path)) {
      return false;
    }

    return true;
  }

  /// \brief
  /// \param path
  bool deserialize(const char *const path) {
    json::node_type json_root;
    if (!json::read_json(path, &json_root)) {
      return false;
    }

    json::node_type json_named_objects_list;
    if (!json::get_child(json_root, json_key::named_objects, &json_named_objects_list)) {
      return false;
    }

    for (const auto &object : json_named_objects_list) {
      name_type name;
      offset_type offset = 0;
      length_type length = 0;
      description_type description;

      if (!json::get_value(object.second, json_key::name, &name) ||
          !json::get_value(object.second, json_key::offset, &offset) ||
          !json::get_value(object.second, json_key::length, &length) ||
          !json::get_value(object.second, json_key::description, &description)) {
        return false;
      }

      if (!insert(name, offset, length, description)) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to reconstruct named object table");
        return false;
      }
    }

    return true;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  enum index {
    name = 0,
    offset = 1,
    length = 2,
    description = 3
  };

  // JSON structure
  // {
  // "named_objects" : [
  //  {"name" : "object0", "offset" : 0x845, "length" : 1, "description" : ""},
  //  {"name" : "object1", "offset" : 0x432, "length" : 2, "description" : "..."}
  // ]
  // }
  struct json_key {
    static constexpr const char *named_objects = "named_objects";
    static constexpr const char *name = "name";
    static constexpr const char *offset = "offset";
    static constexpr const char *length = "length";
    static constexpr const char *description = "description";
  };

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  main_table_type m_main_table;
  key_table_type m_key_table;
};

} // namespace kernel
} // namespace metall
#endif //METALL_DETAIL_NAMED_OBJECT_DIRECTORY_HPP
