// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP
#define METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP

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

namespace metall {
namespace v0 {
namespace kernel {

/// \brief Directory for namaed objects.
/// \tparam offset_type
template <typename offset_type, typename size_type, typename allocator_type>
class named_object_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  static constexpr std::size_t k_max_char_size = 1024;
  using serialized_string_type = std::array<char, k_max_char_size>;
  using mapped_type = std::tuple<serialized_string_type, offset_type, size_type>;
  using key_type = uint64_t;
  using value_type = std::pair<const key_type, mapped_type>;

  using table_type = boost::unordered_multimap<key_type,
                                               mapped_type,
                                               std::hash<key_type>,
                                               std::equal_to<key_type>,
                                               other_allocator_type<value_type>>;
  using const_iterator = typename table_type::const_iterator;

 public:
  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit named_object_directory(const allocator_type &allocator)
      : m_table(allocator) {}
  ~named_object_directory() = default;
  named_object_directory(const named_object_directory &) = default;
  named_object_directory(named_object_directory &&) = default;
  named_object_directory &operator=(const named_object_directory &) = default;
  named_object_directory &operator=(named_object_directory &&) = default;

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  /// \brief
  /// \param name
  /// \param offset
  /// \param length
  /// \return
  bool insert(const std::string &name, const offset_type offset, const size_type length) {
    serialized_string_type serialized_name;
    if (!serialize_string(name, &serialized_name)) {
      return false;
    }

    if (find(name) != end()) return false; // Duplicate element exists

    m_table.emplace(hash_string(name), std::make_tuple(serialized_name, offset, length));
    return true;
  }

  /// \brief
  /// \param name
  /// \return
  const_iterator find(const std::string &name) const {
    auto itr = m_table.find(hash_string(name));

    serialized_string_type serialized_name;
    if (!serialize_string(name, &serialized_name)) {
      return m_table.end();
    }

    for (; itr != m_table.end(); ++itr) {
      if (serialized_name == std::get<0>(itr->second)) {
        return itr;
      }
    }

    return m_table.end();
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
  void erase(const const_iterator position) {
    m_table.erase(position);
  }

  /// \brief
  /// \param path
  bool serialize(const char *const path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    for (const auto &item : m_table) {
      std::stringstream ss;

      ss << item.first; // Key

      // Serialized name
      const auto &serialized_name = std::get<0>(item.second);
      for (std::size_t i = 0; i < serialized_name.size(); ++i) {
        ss << " " << static_cast<uint64_t>(serialized_name[i]);
      }

      ss << " " << static_cast<uint64_t>(std::get<1>(item.second)) // Offset
         << " " << static_cast<uint64_t>(std::get<2>(item.second)); // Length

      ofs << ss.str() << "\n";
      if (!ofs) {
        std::cerr << "Something happened in the ofstream: " << path << std::endl;
        return false;
      }
    }
    ofs.close();

    return true;
  }

  /// \brief
  /// \param path
  bool deserialize(const char *const path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    std::size_t count = 0;
    const std::size_t num_reads_per_item = 1 + std::tuple_size<serialized_string_type>::value + 2;
    uint64_t buf;

    key_type key = key_type();
    serialized_string_type serialized_name = serialized_string_type();
    offset_type offset = 0;
    size_type length = 0;
    while (ifs >> buf) {
      if (count == 0) {
        key = buf;
      } else if (1 <= count && count < num_reads_per_item - 2) {
        serialized_name[count - 1] = static_cast<char>(buf);
      } else if (count == num_reads_per_item - 2) {
        offset = buf;
      } else if (count == num_reads_per_item - 1) {
        length = buf;

        const std::string name = deserialize_string(serialized_name);
        if (key != hash_string(name)) {
          std::cerr << "Something is wrong in the read data" << std::endl;
          return false;
        }

        const bool ret = insert(name, offset, length);
        if (!ret) {
          std::cerr << "Failed to insert" << std::endl;
          return false;
        }
      } else {
        assert(false);
      }

      ++count;
      count %= num_reads_per_item;
    }

    if (!ifs.eof()) {
      std::cerr << "Something happened in the ifstream: " << path << std::endl;
      return false;
    }

    ifs.close();

    return true;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  // -------------------------------------------------------------------------------- //
  // Private methods
  // -------------------------------------------------------------------------------- //
  key_type hash_string(const std::string &name) const {
    return std::hash<std::string>()(std::string(name));
  }

  bool serialize_string(const std::string &name, serialized_string_type *out) const {
    if (name.size() > std::tuple_size<serialized_string_type>::value) {
      std::cerr << "Too long name: " << name << std::endl;
      return false;
    }
    out->fill('\0');
    for (std::size_t i = 0; i < name.size(); ++i) {
      (*out)[i] = name[i];
    }
    return true;
  }

  std::string deserialize_string(const serialized_string_type &serialized_string) const {
    std::string str;
    for (const auto &c : serialized_string) {
      if (c == '\0') break;
      str.push_back(c);
    }
    return str;
  }
  // -------------------------------------------------------------------------------- //
  // Private fields
  // -------------------------------------------------------------------------------- //
  table_type m_table;
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP
