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

#include <boost/container/scoped_allocator.hpp>
#include <boost/container/string.hpp>
#include <boost/unordered_map.hpp>

#include <metall/logger.hpp>
#include <metall/detail/utility/ptree.hpp>
#include <metall/detail/utility/hash.hpp>

namespace metall {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
namespace json = metall::detail::utility::ptree;
}

/// \brief Directory for namaed objects.
/// \tparam offset_type
template <typename _offset_type, typename _size_type, typename allocator_type>
class named_object_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using size_type = _size_type;
  using name_type = std::string;
  using offset_type = _offset_type;
  using length_type = size_type;
  using attribute_type = std::string;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  enum items {
    name = 0,
    offset = 1,
    length = 2,
    attribute = 3
  };

  struct json_key {
    static constexpr const char *named_objects = "named_objects";
    static constexpr const char *name = "name";
    static constexpr const char *offset = "offset";
    static constexpr const char *length = "length";
    static constexpr const char *attribute = "attribute";
  };

  // JSON structure
  // {
  // "named_objects" : [
  //  {"name" : "object0", "offset" : 0x845, "length" : 1, "attribute" : ""},
  //  {"name" : "object0", "offset" : 0x845, "length" : 1, "attribute" : ""}
  // ]
  // }

  using mapped_type = std::tuple<name_type, offset_type, length_type, attribute_type>;
  using key_string_type = name_type;
  using value_type = std::pair<const key_string_type, mapped_type>;
  using table_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<value_type>;
  using table_type = boost::unordered_map<key_string_type,
                                          mapped_type,
                                          util::string_hash < key_string_type>,
  std::equal_to<key_string_type>,
  table_allocator_type>;
  using const_iterator = typename table_type::const_iterator;

 public:

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit named_object_directory(const allocator_type &allocator = allocator_type())
      : m_table(allocator) {}
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
  /// \param attribute
  /// \return
  bool insert(const name_type &name,
              const offset_type offset,
              const length_type length,
              const attribute_type &attribute = std::string()) {
    bool inserted = false;

    try {
      const auto ret = m_table.emplace(name, std::make_tuple(name, offset, length, attribute));
      inserted = ret.second;
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown");
      return false;
    }

    return inserted;
  }

  /// \brief
  /// \param name
  /// \return
  const_iterator find(const name_type &name) const {
    return m_table.find(name);
  }

  size_type count(const name_type &name) const {
    return m_table.count(name);
  }

  /// \brief
  /// \return
  const_iterator begin() const {
    return m_table.begin();
  }

  /// \brief
  /// \return
  const_iterator end() const {
    return m_table.end();
  }

  /// \brief
  /// \param name
  /// \return
  size_type erase(const const_iterator position) {
    if (position == end()) return 0;

    const auto num_erased = m_table.erase(position->first);
    assert(num_erased <= 1);
    return num_erased;
  }

  /// \brief
  /// \param path
  bool serialize(const char *const path) const {

    json::node_type json_named_objects_list;
    for (const auto &item : m_table) {
      json::node_type json_named_object_entry;
      if (!json::add_value(json_key::name, std::get<items::name>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::offset, std::get<items::offset>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::length, std::get<items::length>(item.second), &json_named_object_entry) ||
          !json::add_value(json_key::attribute, std::get<items::attribute>(item.second), &json_named_object_entry)) {
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
      attribute_type attribute;

      if (!json::get_value(object.second, json_key::name, &name) ||
          !json::get_value(object.second, json_key::offset, &offset) ||
          !json::get_value(object.second, json_key::length, &length) ||
          !json::get_value(object.second, json_key::attribute, &attribute)) {
        return false;
      }

      if (!insert(name, offset, length, attribute)) {
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

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  table_type m_table;
};

} // namespace kernel
} // namespace metall
#endif //METALL_DETAIL_NAMED_OBJECT_DIRECTORY_HPP
