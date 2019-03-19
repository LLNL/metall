// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP
#define METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP

#include <iostream>
#include <utility>
#include <string>
#include <unordered_map>
#include <fstream>
#include <cassert>

namespace metall {
namespace v0 {
namespace kernel {

/// \brief Directory for namaed objects.
/// \tparam offset_type
template <typename offset_type, typename size_type>
class named_object_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using key_type = std::string;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using table_type = std::unordered_map<key_type, std::pair<offset_type, size_type>>;
  using const_iterator = typename table_type::const_iterator;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using mapped_type = typename table_type::mapped_type;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  named_object_directory() = default;
  ~named_object_directory() = default;
  named_object_directory(const named_object_directory &) = default;
  named_object_directory(named_object_directory &&) noexcept = default;
  named_object_directory &operator=(const named_object_directory &) = default;
  named_object_directory &operator=(named_object_directory &&) noexcept = default;

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  /// \brief
  /// \param name
  /// \param offset
  /// \param lenght
  /// \return
  bool insert(const key_type &name, const offset_type offset, const size_type lenght) {
    const auto ret = m_table.emplace(name, mapped_type{offset, lenght});
    return ret.second;
  }

  /// \brief
  /// \param name
  /// \param offset
  /// \param lenght
  /// \return
  bool insert(key_type &&name, const offset_type offset, const size_type lenght) {
    const auto ret = m_table.emplace(std::move(name), mapped_type{offset, lenght});
    return ret.second;
  }

  /// \brief
  /// \param name
  /// \return
  const_iterator find(const key_type &name) const {
    return m_table.find(name);
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
  bool serialize(const char *path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    for (const auto &item : m_table) {
      ofs << item.first // Key
          << " " << static_cast<uint64_t>(item.second.first) // Offset
          << " " << static_cast<uint64_t>(item.second.second) << "\n"; // Length
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
  bool deserialize(const char *path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    key_type key;
    uint64_t buf1;
    uint64_t buf2;
    while (ifs >> key >> buf1 >> buf2) {
      auto offset = static_cast<offset_type>(buf1);
      auto length = static_cast<size_type>(buf2);

      const bool ret = insert(std::move(key), offset, length);
      if (!ret) {
        std::cerr << "Duplicate item is found" << std::endl;
        return false;
      }
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

  /// -------------------------------------------------------------------------------- ///
  /// Private methods
  /// -------------------------------------------------------------------------------- ///

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  table_type m_table;
};

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_NAMED_OBJECT_DIRECTORY_HPP
