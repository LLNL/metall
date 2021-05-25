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

#include <metall/container/experiment/json/json_fwd.hpp>
#include <metall/container/experiment/json/value.hpp>

namespace metall::container::experiment::json {

namespace {
namespace bc = boost::container;
}

/// \brief An array is an ordered collection of values.
template <typename _allocator_type = std::allocator<std::byte>>
class array {
 private:
  template <typename alloc, typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<typename std::allocator_traits<alloc>::template rebind_alloc<T>>;

  using value_type = value<_allocator_type>;
  using aray_allocator_type = other_scoped_allocator<_allocator_type, value_type>;
  using array_type = bc::vector<value_type, aray_allocator_type>;

 public:
  using allocator_type = _allocator_type;
  using iterator = typename array_type::iterator;
  using const_iterator = typename array_type::const_iterator;

  explicit array(const allocator_type &alloc = allocator_type())
      : m_array(alloc) {}

  std::size_t size() const noexcept {
    return m_array.size();
  }

  void resize(const std::size_t size) {
    m_array.resize(size, value_type{m_array.get_allocator()});
  }

  value_type &operator[](const std::size_t index) {
    return m_array[index];
  }

  const value_type &operator[](const std::size_t index) const {
    return m_array[index];
  }

  iterator begin() {
    return m_array.begin();
  }

  const_iterator begin() const {
    return m_array.begin();
  }

  iterator end() {
    return m_array.end();
  }

  const_iterator end() const {
    return m_array.end();
  }

 private:
  array_type m_array;
};

template <typename allocator_type>
std::ostream &operator<<(std::ostream &os, const array<allocator_type> &data) {

  os << "[";
  for (std::size_t i = 0; i < data.size(); ++i) {
    if (i > 0) os << ", ";
    os << data[i];
  }
  os << "]";

  return os;
}

} // namespace metall::container::experiment::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_ARRAY_HPP
