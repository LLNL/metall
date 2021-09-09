// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_ARRAY_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_ARRAY_HPP

#include <iostream>
#include <memory>

#include <boost/container/vector.hpp>
#include <boost/container/scoped_allocator.hpp>

#include <metall/container/experimental/json/json_fwd.hpp>

namespace metall::container::experimental::json {

namespace {
namespace bc = boost::container;
}

/// \brief JSON array.
/// An array is an ordered collection of values.
template <typename _allocator_type = std::allocator<std::byte>>
class array {
 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<typename std::allocator_traits<alloc>::template rebind_alloc<
      T>>;

  using value_type = value<_allocator_type>;
  using aray_allocator_type = other_scoped_allocator<_allocator_type, value_type>;
  using array_type = bc::vector<value_type, aray_allocator_type>;

 public:
  using allocator_type = _allocator_type;
  using iterator = typename array_type::iterator;
  using const_iterator = typename array_type::const_iterator;

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit array(const allocator_type &alloc = allocator_type())
      : m_array(alloc) {}

  /// \brief Copy constructor
  array(const array &) = default;

  /// \brief Allocator-extended copy constructor
  array(const array &other, const allocator_type &alloc)
      : m_array(other.m_array, alloc) {}

  /// \brief Move constructor
  array(array &&) noexcept = default;

  /// \brief Allocator-extended move constructor
  array(array &&other, const allocator_type &alloc) noexcept
      : m_array(std::move(other.m_array), alloc) {}

  /// \brief Copy assignment operator
  array &operator=(const array &) = default;

  /// \brief Move assignment operator
  array &operator=(array &&) noexcept = default;

  /// \brief Returns the number of values.
  /// \return The number of vertices.
  std::size_t size() const noexcept {
    return m_array.size();
  }

  /// \brief Change the number of elements stored.
  /// \param size A new size.
  void resize(const std::size_t size) {
    m_array.resize(size, value_type{m_array.get_allocator()});
  }

  /// \brief Access an element.
  /// \param index The index of the element to access.
  /// \return A reference to the element at 'index'.
  value_type &operator[](const std::size_t index) {
    return m_array[index];
  }

  /// \brief Access an element.
  /// \param index The index of the element to access.
  /// \return A const reference to the element at 'index'.
  const value_type &operator[](const std::size_t index) const {
    return m_array[index];
  }

  /// \brief Returns an iterator that is at the beginning of the array.
  /// \return An iterator that is at the beginning of the array.
  iterator begin() {
    return m_array.begin();
  }

  /// \brief Returns an iterator that is at the beginning of the array.
  /// \return A const iterator that is at the beginning of the array.
  const_iterator begin() const {
    return m_array.begin();
  }

  /// \brief Returns an iterator that is at the end of the array.
  /// \return An iterator that is at the end of the array.
  iterator end() {
    return m_array.end();
  }

  /// \brief Returns an iterator that is at the end of the array.
  /// \return A const iterator that is at the end of the array.
  const_iterator end() const {
    return m_array.end();
  }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is returned.
  iterator erase(iterator position) {
    return m_array.erase(position);
  }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is returned.
  iterator erase(const_iterator position) {
    return m_array.erase(position);
  }

 private:
  array_type m_array;
};

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_ARRAY_HPP
