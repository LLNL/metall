// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_CONTAINER_OF_CONTAINERS_ITERATOR_ADAPTOR_HPP
#define METALL_UTILITY_CONTAINER_OF_CONTAINERS_ITERATOR_ADAPTOR_HPP

#include <iterator>

/// \namespace metall::utility
/// \brief Namespace for utility items
namespace metall::utility {

/// \brief Utility class that provides an iterator for a container of
/// containers, e.g., map of vectors This is an experimental implementation and
/// only support forward iterator for now \tparam outer_iterator_type An outer
/// iterator class \tparam inner_iterator_type An inner iterator class
template <typename outer_iterator_type, typename inner_iterator_type>
class container_of_containers_iterator_adaptor {
 public:
  using difference_type =
      typename std::iterator_traits<inner_iterator_type>::difference_type;
  using value_type =
      typename std::iterator_traits<inner_iterator_type>::value_type;
  using pointer = typename std::iterator_traits<inner_iterator_type>::pointer;
  using reference =
      typename std::iterator_traits<inner_iterator_type>::reference;
  using iterator_category =
      std::forward_iterator_tag;  // TODO: expand to bidirectional_iterator_tag

  container_of_containers_iterator_adaptor(outer_iterator_type outer_begin,
                                           outer_iterator_type outer_end)
      : m_outer_iterator(outer_begin),
        m_outer_end(outer_end),
        m_inner_iterator(),
        m_inner_end() {
    if (m_outer_iterator != m_outer_end) {
      m_inner_iterator = std::begin(*m_outer_iterator);
      m_inner_end = std::end(*m_outer_iterator);
      if (m_inner_iterator == m_inner_end) {
        next();
      }
    }
  }

  container_of_containers_iterator_adaptor(outer_iterator_type outer_begin,
                                           inner_iterator_type inner_iterator,
                                           outer_iterator_type outer_end)
      : m_outer_iterator(outer_begin),
        m_outer_end(outer_end),
        m_inner_iterator(inner_iterator),
        m_inner_end() {
    if (m_outer_iterator != m_outer_end) {
      m_inner_end = std::end(*m_outer_iterator);
      if (m_inner_iterator == m_inner_end) {
        next();
      }
    }
  }

  reference operator++() {
    next();
    return *m_inner_iterator;
  }

  container_of_containers_iterator_adaptor operator++(int) {
    container_of_containers_iterator_adaptor tmp(*this);
    operator++();
    return tmp;
  }

  pointer operator->() { return &(*m_inner_iterator); }

  reference operator*() { return (*m_inner_iterator); }

  bool equal(const container_of_containers_iterator_adaptor &other) const {
    return (m_outer_iterator == m_outer_end &&
            other.m_outer_iterator == other.m_outer_end) ||
           (m_outer_iterator == other.m_outer_iterator &&
            m_inner_iterator == other.m_inner_iterator);
  }

 private:
  void next() {
    if (m_outer_iterator == m_outer_end) {
      return;  // already at the outer end
    }

    if (m_inner_iterator != m_inner_end) {
      m_inner_iterator = std::next(m_inner_iterator);
      if (m_inner_iterator != m_inner_end) {
        return;  // found the next one
      }
    }

    // iterate until find the next valid element
    while (true) {
      m_outer_iterator =
          std::next(m_outer_iterator);  // move to the next container
      if (m_outer_iterator == m_outer_end) {
        return;  // reach the outer end
      }

      // Check if the current container is empty
      m_inner_iterator = std::begin(*m_outer_iterator);
      m_inner_end = std::end(*m_outer_iterator);
      if (m_inner_iterator != m_inner_end) {
        return;  // found the next one
      }
    }
  }

  outer_iterator_type m_outer_iterator;
  outer_iterator_type m_outer_end;
  inner_iterator_type m_inner_iterator;
  inner_iterator_type m_inner_end;
};

template <typename outer_iterator_type, typename inner_iterator_type>
inline bool operator==(
    const container_of_containers_iterator_adaptor<outer_iterator_type,
                                                   inner_iterator_type> &lhs,
    const container_of_containers_iterator_adaptor<outer_iterator_type,
                                                   inner_iterator_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename outer_iterator_type, typename inner_iterator_type>
inline bool operator!=(
    const container_of_containers_iterator_adaptor<outer_iterator_type,
                                                   inner_iterator_type> &lhs,
    const container_of_containers_iterator_adaptor<outer_iterator_type,
                                                   inner_iterator_type> &rhs) {
  return !(lhs == rhs);
}

}  // namespace metall::utility

#endif  // METALL_UTILITY_CONTAINER_OF_CONTAINERS_ITERATOR_ADAPTOR_HPP
