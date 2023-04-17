// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_JSON_ARRAY_HPP
#define METALL_JSON_ARRAY_HPP

#include <iostream>
#include <memory>
#include <algorithm>

#include <metall/container/vector.hpp>
#include <metall/container/scoped_allocator.hpp>
#include <metall/json/json_fwd.hpp>

namespace metall::json {

namespace {
namespace mc = metall::container;
}

namespace jsndtl {

/// \brief Provides 'equal' calculation for other array types that have the same
/// interface as the array class.
template <typename allocator_type, typename other_array_type>
inline bool general_array_equal(const array<allocator_type> &array,
                                const other_array_type &other_array) noexcept {
  if (array.size() != other_array.size()) return false;
  return std::equal(array.begin(), array.end(), other_array.begin());
}

}  // namespace jsndtl

/// \brief JSON array.
/// An array is an ordered collection of values.
#ifdef DOXYGEN_SKIP
template <typename Alloc = std::allocator<std::byte>>
#else
template <typename Alloc>
#endif
class array {
 public:
  using value_type = value<Alloc>;

 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = mc::scoped_allocator_adaptor<
      typename std::allocator_traits<alloc>::template rebind_alloc<T>>;
  using aray_allocator_type = other_scoped_allocator<Alloc, value_type>;
  using array_type = mc::vector<value_type, aray_allocator_type>;

 public:
  using allocator_type = Alloc;
  using iterator = typename array_type::iterator;
  using const_iterator = typename array_type::const_iterator;
  using reference = value_type &;
  using const_reference = const value_type &;

  /// \brief Constructor
  array() = default;

  /// \brief Constructor.
  /// \param alloc An allocator object.
  explicit array(const allocator_type &alloc) : m_array(alloc) {}

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

  /// \brief Swap contents.
  void swap(array &other) noexcept {
    using std::swap;
    swap(m_array, other.m_array);
  }

  /// \brief Returns the number of values.
  /// \return The number of vertices.
  std::size_t size() const noexcept { return m_array.size(); }

  /// \brief Returns the number of values that can be held in currently
  /// allocated storage. \return The number of values that can be held in
  /// currently allocated storage.
  std::size_t capacity() const noexcept { return m_array.capacity(); }

  /// \brief Change the number of elements stored.
  /// \param size A new size.
  void resize(const std::size_t size) {
    m_array.resize(size, value_type{m_array.get_allocator()});
  }

  /// \brief Clear the contents.
  void clear() { m_array.clear(); }

  /// \brief Access an element.
  /// \param index The index of the element to access.
  /// \return A reference to the element at 'index'.
  reference operator[](const std::size_t index) { return m_array[index]; }

  /// \brief Access an element.
  /// \param index The index of the element to access.
  /// \return A const reference to the element at 'index'.
  const_reference operator[](const std::size_t index) const {
    return m_array[index];
  }

  /// \brief Returns an iterator that is at the beginning of the array.
  /// \return An iterator that is at the beginning of the array.
  iterator begin() { return m_array.begin(); }

  /// \brief Returns an iterator that is at the beginning of the array.
  /// \return A const iterator that is at the beginning of the array.
  const_iterator begin() const { return m_array.begin(); }

  /// \brief Returns an iterator that is at the end of the array.
  /// \return An iterator that is at the end of the array.
  iterator end() { return m_array.end(); }

  /// \brief Returns an iterator that is at the end of the array.
  /// \return A const iterator that is at the end of the array.
  const_iterator end() const { return m_array.end(); }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is
  /// returned.
  iterator erase(iterator position) { return m_array.erase(position); }

  /// \brief Erases the element at 'position'.
  /// \param position The position of the element to erase.
  /// \return Iterator following the removed element.
  /// If 'position' refers to the last element, then the end() iterator is
  /// returned.
  iterator erase(const_iterator position) { return m_array.erase(position); }

  /// \brief Add an element to the end of the array.
  /// Expand (resize) the array if capacity() < size() + 1.
  /// \param value Value to add.
  void push_back(const value_type &value) { m_array.push_back(value); }

  /// \brief Add an element to the end of the array.
  /// Expand (resize) the array if capacity() < size() + 1.
  /// \param value Value to add.
  void push_back(value_type &&value) { m_array.push_back(std::move(value)); }

  /// \brief Return `true` if two arrays are equal.
  /// Arrays are equal when their sizes are the same, and they are
  /// element-for-element equal in order. \param lhs An array to compare. \param
  /// rhs An array to compare. \return True if two arrays are equal. Otherwise,
  /// false.
  friend bool operator==(const array &lhs, const array &rhs) noexcept {
    return jsndtl::general_array_equal(lhs, rhs);
  }

  /// \brief Return `true` if two arrays are not equal.
  /// Arrays are equal when their sizes are the same, and they are
  /// element-for-element equal in order. \param lhs An array to compare. \param
  /// rhs An array to compare. \return True if two arrays are not equal.
  /// Otherwise, false.
  friend bool operator!=(const array &lhs, const array &rhs) noexcept {
    return !(lhs == rhs);
  }

  /// \brief Return an allocator object.
  allocator_type get_allocator() const noexcept {
    return m_array.get_allocator();
  }

 private:
  array_type m_array{allocator_type{}};
};

/// \brief Swap value instances.
template <typename allocator_type>
inline void swap(array<allocator_type> &lhd,
                 array<allocator_type> &rhd) noexcept {
  lhd.swap(rhd);
}

}  // namespace metall::json

#endif  // METALL_JSON_ARRAY_HPP
