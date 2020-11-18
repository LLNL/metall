// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_ATTRIBUTED_OBJECT_DIRECTORY_HPP
#define METALL_DETAIL_ATTRIBUTED_OBJECT_DIRECTORY_HPP

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

/// \brief Directory for attributed objects.
/// \tparam _offset_type
/// \tparam _size_type
template <typename _offset_type, typename _size_type>
class attributed_object_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using size_type = _size_type;
  using name_type = std::string;
  using offset_type = _offset_type;
  using length_type = size_type;
  using description_type = std::string;

  struct entry_type {
    name_type name;
    offset_type offset;
    length_type length;
    description_type description;

    bool equal(const entry_type &other) const {
      return (name == other.name);
    }
  };

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  struct entry_hash {
    uint64_t operator()(const entry_type &entry) const {
      return util::hash<offset_type>()(entry.offset);
    }
  };
  struct entry_equal {
    bool operator()(const entry_type &lhd, const entry_type &rhd) const {
      return lhd.equal(rhd);
    }
  };
  using entry_table_type = boost::unordered_set<entry_type, entry_hash, entry_equal>;

  // Following tables hold the index of the corresponding entry of each key
  using offset_index_table_type = boost::unordered_map<offset_type,
                                                       typename entry_table_type::iterator,
                                                       util::hash<offset_type>>;
  using name_index_table_type = boost::unordered_map<name_type,
                                                     typename entry_table_type::iterator,
                                                     util::string_hash<name_type>>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using const_iterator = typename entry_table_type::const_iterator;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  attributed_object_directory() = default;
  ~attributed_object_directory() noexcept = default;
  attributed_object_directory(const attributed_object_directory &) = default;
  attributed_object_directory(attributed_object_directory &&) noexcept = default;
  attributed_object_directory &operator=(const attributed_object_directory &) = default;
  attributed_object_directory &operator=(attributed_object_directory &&) noexcept = default;

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

    if (m_offset_index_table.count(offset) > 0) {
      assert(m_name_index_table.count(name) > 0);
      return false;
    }
    assert(m_name_index_table.count(name) == 0);

    try {
      const auto inserted_ret = m_entry_table.insert(entry_type{name, offset, length, description});
      assert(inserted_ret.second);
      {
        [[maybe_unused]] const auto ret = m_offset_index_table.emplace(offset, inserted_ret.first);
        assert(ret.first != m_offset_index_table.end());
        assert(ret.second);
      }
      {
        [[maybe_unused]] const auto ret = m_name_index_table.emplace(name, inserted_ret.first);
        assert(ret.first != m_name_index_table.end());
        assert(ret.second);
      }
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when inserting entry");
      return false;
    }

    return true;
  }

  /// \brief
  /// \return
  size_type size() const {
    return m_entry_table.size();
  }

  /// \brief Counts by name
  /// \param name
  /// \return
  size_type count(const name_type &name) const {
    return m_name_index_table.count(name);
  }

  /// \brief Count by offset
  /// \param offset
  /// \return
  size_type count(const offset_type &offset) const {
    return m_offset_index_table.count(offset);
  }

  /// \brief Finds by name
  /// \param name
  /// \return
  const_iterator find(const name_type &name) const {
    if (count(name) > 0) {
      auto itr = m_name_index_table.find(name);
      assert(itr->second != m_entry_table.end());
      return const_iterator(itr->second);
    }
    return m_entry_table.cend();
  }

  /// \brief Finds by offset
  /// \param offset
  /// \return
  const_iterator find(const offset_type &offset) const {
    if (count(offset) > 0) {
      auto itr = m_offset_index_table.find(offset);
      assert(itr->second != m_entry_table.end());
      return const_iterator(itr->second);
    }
    return m_entry_table.cend();
  }

  /// \brief
  /// \return
  const_iterator begin() const {
    return m_entry_table.cbegin();
  }

  /// \brief
  /// \return
  const_iterator end() const {
    return m_entry_table.cend();
  }

  /// \brief Erase by iterator
  /// \param position
  /// \return
  size_type erase(const_iterator position) {
    if (position == m_entry_table.end())
      return 0;

    try {
      m_offset_index_table.erase(position->offset);
      m_name_index_table.erase(position->name);
      m_entry_table.erase(position);
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when erasing an entry");
      return 0;
    }

    return 1;
  }

  /// \brief Erase by name
  /// \param name
  /// \return
  size_type erase(const name_type &name) {
    if (count(name) == 0)
      return 0;

    try {
      auto itr = find(name);
      m_offset_index_table.erase(itr->offset);
      m_name_index_table.erase(itr->name);
      m_entry_table.erase(itr);
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when erasing an entry");
      return 0;
    }

    return 1;
  }

  /// \brief Erase offset
  /// \param offset
  /// \return
  size_type erase(const offset_type &offset) {
    if (count(offset) == 0)
      return 0;

    try {
      auto itr = find(offset);
      m_offset_index_table.erase(itr->offset);
      m_name_index_table.erase(itr->name);
      m_entry_table.erase(itr);
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when erasing an entry");
      return 0;
    }

    return 1;
  }

  /// \brief
  /// \param path
  bool serialize(const char *const path) const {

    json::node_type json_named_objects_list;
    for (const auto &item : m_entry_table) {
      json::node_type json_named_object_entry;
      if (!json::add_value(json_key::name, item.name, &json_named_object_entry) ||
          !json::add_value(json_key::offset, item.offset, &json_named_object_entry) ||
          !json::add_value(json_key::length, item.length, &json_named_object_entry) ||
          !json::add_value(json_key::description, item.description, &json_named_object_entry)) {
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
  entry_table_type m_entry_table;
  offset_index_table_type m_offset_index_table;
  name_index_table_type m_name_index_table;
};
} // namespace kernel
} // namespace metall
#endif //METALL_DETAIL_ATTRIBUTED_OBJECT_DIRECTORY_HPP
