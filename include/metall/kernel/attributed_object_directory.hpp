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
#include <boost/container/list.hpp>

#include <metall/logger.hpp>
#include <metall/detail/ptree.hpp>
#include <metall/detail/hash.hpp>

namespace metall {
namespace kernel {

namespace {
namespace mdtl = metall::mtlldetail;
namespace json = metall::mtlldetail::ptree;
}

template <typename T>
inline auto gen_type_name() {
  return typeid(T).name();
}

template <typename T>
inline auto gen_type_id() {
  return typeid(T).hash_code();
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
  using type_id_type = decltype(gen_type_id<void>());
  using description_type = std::string;

  class entry_type {
   public:
    entry_type() = default;
    entry_type(const name_type &name, const offset_type &offset, const length_type &length,
               const type_id_type &type_id, const description_type &description)
        : m_name(name),
          m_offset(offset),
          m_length(length),
          m_type_id(type_id),
          m_description(description) {}

    ~entry_type() = default;

    entry_type(const entry_type &) = default;
    entry_type(entry_type &&) noexcept = default;

    entry_type &operator=(const entry_type &) = default;
    entry_type &operator=(entry_type &&) noexcept = default;

    const auto &name() const {
      return m_name;
    }

    const auto &offset() const {
      return m_offset;
    }

    const auto &length() const {
      return m_length;
    }

    const auto &type_id() const {
      return m_type_id;
    }

    const auto &description() const {
      return m_description;
    }

    auto &description() {
      return m_description;
    }

    template <typename T>
    bool is_type() const {
      return m_type_id == gen_type_id<T>();
    }

   private:
    name_type m_name;
    offset_type m_offset;
    length_type m_length;
    type_id_type m_type_id;
    description_type m_description;
  };

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using entry_table_type = boost::container::list<entry_type>;

  // Following tables hold the index of the corresponding entry of each key
  using offset_index_table_type = boost::unordered_map<offset_type,
                                                       typename entry_table_type::iterator,
                                                       mdtl::hash<offset_type>>;
  using name_index_table_type = boost::unordered_map<name_type,
                                                     typename entry_table_type::iterator,
                                                     mdtl::string_hash<name_type>>;

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
  /// \param name (can be empty)
  /// \param offset
  /// \param length
  /// \param type_id
  /// \param description
  /// \return
  bool insert(const name_type &name,
              const offset_type offset,
              const length_type length,
              const type_id_type type_id,
              const description_type &description = std::string()) {

    if (m_offset_index_table.count(offset) > 0) {
      assert(name.empty() || m_name_index_table.count(name) > 0);
      return false;
    }
    assert(name.empty() || m_name_index_table.count(name) == 0);

    try {
      auto inserted_itr = m_entry_table.emplace(m_entry_table.end(),
                                                entry_type{name, offset, length, type_id, description});
      {
        [[maybe_unused]] const auto ret = m_offset_index_table.emplace(offset, inserted_itr);
        assert(ret.first != m_offset_index_table.end());
        assert(ret.second);
      }
      if (!name.empty()) {
        [[maybe_unused]] const auto ret = m_name_index_table.emplace(name, inserted_itr);
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
  /// \param position
  /// \param description
  /// \return
  bool set_description(const_iterator position, const description_type &description) {
    if (position == m_entry_table.cend()) return false;

    auto entry_itr = m_offset_index_table.find(position->offset())->second;
    assert(entry_itr != m_entry_table.end());

    entry_itr->description() = description;

    return true;
  }

  /// \brief
  /// \param position
  /// \param description
  /// \return
  bool get_description(const_iterator position, description_type *description) const {
    if (position == m_entry_table.cend()) return false;

    auto entry_itr = m_offset_index_table.find(position->offset())->second;
    assert(entry_itr != m_entry_table.end());

    description->assign(entry_itr->description());

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
      m_offset_index_table.erase(position->offset());
      m_entry_table.erase(position);
      if (!position->name().empty()) m_name_index_table.erase(position->name());
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
      m_offset_index_table.erase(itr->offset());
      m_entry_table.erase(itr);
      if (!name.empty()) m_name_index_table.erase(name);
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
      m_offset_index_table.erase(itr->offset());
      m_entry_table.erase(itr);
      if (!itr->name().empty()) m_name_index_table.erase(itr->name());
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when erasing an entry");
      return 0;
    }

    return 1;
  }

  /// \brief Clears tables
  /// \return
  bool clear() {
    try {
      m_offset_index_table.clear();
      m_name_index_table.clear();
      m_entry_table.clear();
    } catch (...) {
      logger::out(logger::level::critical, __FILE__, __LINE__, "Exception was thrown when clearing entries");
      return false;
    }
    return true;
  }

  /// \brief
  /// \param path
  bool serialize(const char *const path) const {

    json::node_type json_attributed_objects_list;
    for (const auto &item : m_entry_table) {
      json::node_type json_named_object_entry;
      if (!json::add_value(json_key::name, item.name(), &json_named_object_entry) ||
          !json::add_value(json_key::offset, item.offset(), &json_named_object_entry) ||
          !json::add_value(json_key::length, item.length(), &json_named_object_entry) ||
          !json::add_value(json_key::type_id, item.type_id(), &json_named_object_entry) ||
          !json::add_value(json_key::description, item.description(), &json_named_object_entry)) {
        return false;
      }
      if (!json::push_back(json_named_object_entry, &json_attributed_objects_list)) {
        return false;
      }
    }

    json::node_type json_root;
    if (!json::add_child(json_key::attributed_objects, json_attributed_objects_list, &json_root)) {
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

    json::node_type json_attributed_objects_list;
    if (!json::get_child(json_root, json_key::attributed_objects, &json_attributed_objects_list)) {
      return false;
    }

    for (const auto &object : json_attributed_objects_list) {
      name_type name;
      offset_type offset = 0;
      length_type length = 0;
      description_type description;
      type_id_type type_id;

      if (!json::get_value(object.second, json_key::name, &name) ||
          !json::get_value(object.second, json_key::offset, &offset) ||
          !json::get_value(object.second, json_key::length, &length) ||
          !json::get_value(object.second, json_key::type_id, &type_id) ||
          !json::get_value(object.second, json_key::description, &description)) {
        return false;
      }

      if (count(name) > 0) {
        logger::out(logger::level::error, __FILE__, __LINE__, "Failed to reconstruct object table");
        return false;
      }

      if (!insert(name, offset, length, type_id, description)) {
        logger::out(logger::level::critical, __FILE__, __LINE__, "Failed to reconstruct object table");
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
  // "attributed_objects" : [
  //  {"name" : "object0", "offset" : 0x845, "length" : 1, "type_id" : "424", "description" : ""},
  //  {"name" : "object1", "offset" : 0x432, "length" : 2, "type_id" : "932", "description" : "..."}
  // ]
  // }
  struct json_key {
    static constexpr const char *attributed_objects = "attributed_objects";
    static constexpr const char *name = "name";
    static constexpr const char *offset = "offset";
    static constexpr const char *length = "length";
    static constexpr const char *type_id = "type_id";
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
