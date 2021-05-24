// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_OBJECT_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_OBJECT_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <string_view>

#include <boost/container/vector.hpp>
#include <boost/unordered_set.hpp>

#include <metall/utility/hash.hpp>
#include <metall/container/experiment/json/json_fwd.hpp>
#include <metall/container/experiment/json/value.hpp>
#include <metall/container/experiment/json/string.hpp>
#include <metall/container/experiment/json/key_value_pair.hpp>

namespace metall::container::experiment::json {

namespace {
namespace bc = boost::container;
}

/// \brief JSON object
/// An object is an unordered map of name and value pairs.
template <typename _allocator_type = std::allocator<std::byte>>
class object {
 public:
  using allocator_type = _allocator_type;
  using value_type = key_value_pair<allocator_type>;
  using key_type = std::string_view; //typename value_type::key_type;
  using mapped_type = value<allocator_type>; //typename value_type::value_type;

 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<typename std::allocator_traits<alloc>::template rebind_alloc<T>>;

  using value_storage_alloc_type = other_scoped_allocator<allocator_type, value_type>;
  using value_storage_type = bc::vector<value_type, value_storage_alloc_type>;

  // Key: the hash value of the corresponding key in the value_storage
  using index_key_type = uint64_t;
  // Value: the position of the corresponding item in the value_storage
  using value_postion_type = typename value_storage_type::size_type;
  using index_table_allocator_type = other_scoped_allocator<allocator_type,
                                                                    std::pair<const index_key_type,
                                                                              value_postion_type>>;
  using index_table_type = boost::unordered_multimap<index_key_type,
                                                     typename value_storage_type::size_type,
                                                     boost::hash<index_key_type>,
                                                     std::equal_to<>,
                                                     index_table_allocator_type>;

 public:

  using iterator = typename value_storage_type::iterator;
  using const_iterator = typename value_storage_type::const_iterator;

  explicit object(const allocator_type &alloc = allocator_type())
      : m_index_table(alloc),
        m_value_storage(alloc) {}

  mapped_type &operator[](std::string_view key) {
    const auto pos = priv_locate_value(key);
    if (pos < m_value_storage.max_size()) {
      return m_value_storage[pos].value();
    }

    // Allocates a new value with the key
    const auto hash = metall::mtlldetail::MurmurHash64A(key.data(), key.length(), 123);
    m_value_storage.emplace_back(key, mapped_type{m_value_storage.get_allocator()});
    m_index_table.emplace(hash, m_value_storage.size() - 1);
    return m_value_storage.back().value();
  }

  const mapped_type &operator[](std::string_view key) const {
    return m_value_storage[priv_locate_value(key)].value();
  }

  iterator begin() {
    return m_value_storage.begin();
  }

  const_iterator begin() const {
    return m_value_storage.begin();
  }

  iterator end() {
    return m_value_storage.end();
  }

  const_iterator end() const {
    return m_value_storage.end();
  }

 private:

  value_postion_type priv_locate_value(std::string_view key) const {
    const auto hash = metall::mtlldetail::MurmurHash64A(key.data(), key.length(), 123);
    for (auto itr = m_index_table.find(hash), end = m_index_table.end(); itr != end; ++itr) {
      const auto value_pos = itr->second;
      if (m_value_storage[value_pos].key() == key) {
        return value_pos; // Found the key
      }
    }
    return m_value_storage.max_size(); // Couldn't find
  }

  index_table_type m_index_table;
  value_storage_type m_value_storage;
};

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const object<allocator_type> &obj) {
  os << "{\n";

  bool is_first = true;
  for (const auto &elem : obj) {
    if (!is_first) {
      os << ",\n";
    }
    os << "\"" << elem.key() << "\" : " << elem.value();
    is_first = false;
  }

  os << "\n}";

  return os;
}

} // namespace metall::container::experiment::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_OBJECT_HPP
