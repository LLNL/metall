// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP
#define METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP

#include <type_traits>

#include <metall/kernel/attributed_object_directory.hpp>

namespace metall {
namespace detail {

/// \brief
/// \tparam _offset_type
/// \tparam _size_type
template <typename _offset_type, typename _size_type>
class general_named_object_attr_accessor {
 private:
  using object_directory_type = kernel::attributed_object_directory<_offset_type, _size_type>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using size_type = typename object_directory_type::size_type;
  using name_type = typename object_directory_type::name_type;
  using offset_type = typename object_directory_type::offset_type;
  using length_type = typename object_directory_type::length_type;
  using description_type = typename object_directory_type::description_type;

  using const_iterator = typename object_directory_type::const_iterator;

  explicit general_named_object_attr_accessor(const std::string &object_attribute_file_path)
      : m_object_directory(),
        m_object_attribute_file_path(object_attribute_file_path),
        m_good_status(false) {
    m_good_status = m_object_directory.deserialize(m_object_attribute_file_path.c_str());
  }

  /// \brief Returns if the internal state is good.
  /// \return Returns true if good; otherwise, false.
  bool good() const {
    return m_good_status;
  }

  /// \brief Returns the number of objects in the directory.
  /// \return Returns the number of objects in the directory.
  size_type num_objects() const {
    return m_object_directory.size();
  }

  /// \brief Counts the number of objects with the name.
  /// As object name must be unique, only 1 or 0 is returned.
  /// \param name A name of an object to count.
  /// \return Returns 1 if the object exist; otherwise, 0.
  size_type count(const char* name) const {
    return m_object_directory.count(name);
  }

  /// \brief Finds the position of the object attribute with 'name'.
  /// \param name A name of an object to find.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  const_iterator find(const char* name) const {
    return m_object_directory.find(name);
  }

  /// \brief Returns a const iterator that points the beginning of stored object attribute.
  /// \return Returns a const iterator that points the beginning of stored object attribute.
  const_iterator begin() const {
    return m_object_directory.begin();
  }

  /// \brief Returns a const iterator that points the end of stored object attribute.
  /// \return Returns a const iterator that points the end of stored object attribute.
  const_iterator end() const {
    return m_object_directory.end();
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  bool set_description(const_iterator position, const description_type &description) {
    if (position == end()) return false;

    if (!m_object_directory.set_description(position, description)
        || !m_object_directory.serialize(m_object_attribute_file_path.c_str())) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Filed to set description");
      m_good_status = false;
      return false;
    }

    return true;
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param name A name of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  bool set_description(const char* name, const description_type &description) {
    return set_description(find(name), description);
  }

 private:
  object_directory_type m_object_directory;
  std::string m_object_attribute_file_path;
  bool m_good_status;
};
} // namespace detail

/// \brief Objet attribute accessor for named object.
/// \tparam _offset_type Offset type.
/// \tparam _size_type Size type.
template <typename _offset_type, typename _size_type>
class named_object_attr_accessor : public detail::general_named_object_attr_accessor<_offset_type, _size_type> {

 private:
  using base_type = detail::general_named_object_attr_accessor<_offset_type, _size_type>;

 public:
  using size_type = typename base_type::size_type;
  using name_type = typename base_type::name_type;
  using offset_type = typename base_type::offset_type;
  using length_type = typename base_type::length_type;
  using description_type = typename base_type::description_type;
  using const_iterator = typename base_type::const_iterator;

  named_object_attr_accessor() = delete;

  explicit named_object_attr_accessor(const std::string &object_attribute_file_path)
      : base_type(object_attribute_file_path) {}
};

/// \brief Objet attribute accessor for unique object.
/// \tparam _offset_type Offset type.
/// \tparam _size_type Size type.
template <typename _offset_type, typename _size_type>
class unique_object_attr_accessor : public detail::general_named_object_attr_accessor<_offset_type, _size_type> {

 private:
  using base_type = detail::general_named_object_attr_accessor<_offset_type, _size_type>;

 public:
  using size_type = typename base_type::size_type;
  using name_type = typename base_type::name_type;
  using offset_type = typename base_type::offset_type;
  using length_type = typename base_type::length_type;
  using description_type = typename base_type::description_type;
  using const_iterator = typename base_type::const_iterator;

  unique_object_attr_accessor() = delete;

  explicit unique_object_attr_accessor(const std::string &object_attribute_file_path)
      : base_type(object_attribute_file_path) {}

  /// \brief Counts the number of objects with the name.
  /// As object name must be unique, only 1 or 0 is returned.
  /// \param name A name of an object to count.
  /// \return Returns 1 if the object exist; otherwise, 0.
  size_type count(const char* name) const {
    return base_type::count(name);
  }

  /// \brief Counts the number of the unique object of type T with the name, i.e., 1 or 0 is returned.
  /// \tparam T A type of an object.
  /// \return Returns 1 if the object exist; otherwise, 0.
  template <typename T>
  size_type count(const decltype(unique_instance) &) const {
    return base_type::count(typeid(T).name());
  }

  /// \brief Finds the position of the object attribute with 'name'.
  /// \param name A name of an object to find.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  const_iterator find(const char* name) const {
    return base_type::find(name);
  }

  /// \brief Finds the position of the attribute of the unique object of type T.
  /// \tparam T A type of an object.
  /// \return Returns a const iterator that points the found object attribute.
  /// If not found, a returned iterator is equal to that of end().
  template <typename T>
  const_iterator find(const decltype(unique_instance) &) const {
    return base_type::find(typeid(T).name());
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  bool set_description(const_iterator position, const description_type &description) {
    return base_type::set_description(position, description);
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param name A name of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  bool set_description(const char* name, const description_type &description) {
    return base_type::set_description(name, description);
  }

  /// \brief Sets an description to the unique object of type T.
  /// An existing one is overwrite.
  /// \tparam T A type of an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  template <typename T>
  bool set_description(const decltype(unique_instance) &, const description_type &description) {
    return base_type::set_description(typeid(T).name(), description);
  }
};

/// \brief
/// \tparam _offset_type
/// \tparam _size_type
template <typename _offset_type, typename _size_type>
class anonymous_object_attr_accessor {
 private:
  using object_directory_type = kernel::attributed_object_directory<_offset_type, _size_type>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using size_type = typename object_directory_type::size_type;
  using name_type = typename object_directory_type::name_type;
  using offset_type = typename object_directory_type::offset_type;
  using length_type = typename object_directory_type::length_type;
  using description_type = typename object_directory_type::description_type;

  using const_iterator = typename object_directory_type::const_iterator;

  anonymous_object_attr_accessor() = delete;

  explicit anonymous_object_attr_accessor(const std::string &object_attribute_file_path)
      : m_object_directory(),
        m_object_attribute_file_path(object_attribute_file_path),
        m_good_status(false) {
    m_good_status = m_object_directory.deserialize(m_object_attribute_file_path.c_str());
  }

  /// \brief Returns if the internal state is good.
  /// \return Returns true if good; otherwise, false.
  bool good() const {
    return m_good_status;
  }

  /// \brief Returns the number of objects in the directory.
  /// \return Returns the number of objects in the directory.
  size_type num_objects() const {
    return m_object_directory.size();
  }

  /// \brief Returns a const iterator that points the beginning of stored object attribute.
  /// \return Returns a const iterator that points the beginning of stored object attribute.
  const_iterator begin() const {
    return m_object_directory.begin();
  }

  /// \brief Returns a const iterator that points the end of stored object attribute.
  /// \return Returns a const iterator that points the end of stored object attribute.
  const_iterator end() const {
    return m_object_directory.end();
  }

  /// \brief Sets an description.
  /// An existing one is overwrite.
  /// \param position An iterator to an object.
  /// \param description A description string in description_type.
  /// \return Returns true if a description is set (stored) successfully.
  /// Otherwise, false.
  bool set_description(const_iterator position, const description_type &description) {
    if (position == end()) return false;

    if (!m_object_directory.set_description(position, description)
        || !m_object_directory.serialize(m_object_attribute_file_path.c_str())) {
      logger::out(logger::level::error, __FILE__, __LINE__, "Filed to set description");
      m_good_status = false;
      return false;
    }

    return true;
  }

 private:
  object_directory_type m_object_directory;
  std::string m_object_attribute_file_path;
  bool m_good_status;
};
} // namespace metall

#endif //METALL_KERNEL_OBJECT_ATTRIBUTE_ACCESSOR_HPP
