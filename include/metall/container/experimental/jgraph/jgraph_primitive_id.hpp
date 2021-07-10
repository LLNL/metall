// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_PRIMITIVE_ID_HPP
#define METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_PRIMITIVE_ID_HPP

#include <iostream>
#include <string_view>
#include <functional>
#include <utility>
#include <optional>

#include <boost/unordered_map.hpp>
#include <boost/container/vector.hpp>

#include <metall/utility/hash.hpp>
#include <metall/container/experimental/json/json.hpp>

/// \namespace metall::container::experimental::jgraph
/// \brief Namespace for Metall JSON graph container, which is in an experimental phase.
namespace metall::container::experimental::jgraph {

namespace {
namespace bc = boost::container;
namespace mj = metall::container::experimental::json;
}

// --- Forward declarations --- //
template <typename vertex_key_type, typename allocator_type>
class jgraph_primitive_id;

namespace jgdtl {
template <typename vertex_table_iterator_type>
class vertex_iterator_impl;

template <typename edge_table_iterator_type>
class edge_iterator_impl;
} // namespace jgdtl

/// \brief JSON Graph which can be used with Metall.
/// Supported graph type:
/// There is a single 'JSON value' data per vertex and edge
/// Every vertex and edge has an unique ID.
/// \tparam allocator_type Allocator type.
template <typename _key_type = uint64_t, typename _allocator_type = std::allocator<std::byte>>
class jgraph_primitive_id {
 public:
  /// \brief Vertex ID type.
  using key_type = _key_type;

  using allocator_type = _allocator_type;

  /// \brief JSON value type every vertex and edge has.
  using value_type = mj::value<allocator_type>;

 private:
  template <typename T>
  using other_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<other_allocator<T>>;

  class key_value_pair_type {
   public:

//    key_value_pair_type(const allocator_type &allocator)
//        : m_value(allocator) {}

    key_value_pair_type(const key_type &key, const value_type &value) :
        m_key(key),
        m_value(value) {}

    key_value_pair_type(key_type &&key, value_type &&value) :
        m_key(key),
        m_value(std::move(value)) {}

    key_value_pair_type(const key_value_pair_type &) = default;
    key_value_pair_type(key_value_pair_type &&) noexcept = default;

    key_value_pair_type(const key_value_pair_type &other, allocator_type alloc)
        : m_key(other.m_key),
          m_value(other.m_value, alloc) {}

    key_value_pair_type(key_value_pair_type &&other, allocator_type alloc) noexcept
        : m_key(std::move(other.m_key)),
          m_value(std::move(other.m_value), alloc) {}

    const key_type &key() const {
      return m_key;
    }

    value_type &value() {
      return m_value;
    }

    const value_type &value() const {
      return m_value;
    }

   private:
    key_type m_key;
    value_type m_value;
  };

  struct edge_table_value_type {
    using allocator_type = _allocator_type;
    using edge_data_type = key_value_pair_type;

    explicit edge_table_value_type(const allocator_type &alloc = allocator_type())
        : edge_data(alloc) {}

    explicit edge_table_value_type(const edge_data_type &edge_data)
        : edge_data(edge_data) {}

    explicit edge_table_value_type(edge_data_type &&edge_data)
        : edge_data(std::move(edge_data)) {}

    edge_table_value_type(const edge_table_value_type &) = default;
    edge_table_value_type(edge_table_value_type &&) noexcept = default;

    edge_table_value_type(const edge_table_value_type &other, allocator_type alloc)
        : edge_data(other.edge_data, alloc) {}

    edge_table_value_type(edge_table_value_type &&other, allocator_type alloc) noexcept
        : edge_data(std::move(other.edge_data), alloc) {}

    edge_data_type edge_data;
  };

  using edge_table_type = boost::unordered_multimap<key_type,
                                                    edge_table_value_type,
                                                    metall::utility::hash<key_type>,
                                                    std::equal_to<>,
                                                    other_scoped_allocator<std::pair<const key_type,
                                                                                     edge_table_value_type>>>;

  struct vertex_table_value_type {
    using allocator_type = _allocator_type;
    using vertex_data_type = key_value_pair_type;

    explicit vertex_table_value_type(const key_type vertex_id, const allocator_type &alloc = allocator_type())
        : vertex_data(vertex_id, value_type{alloc}),
          edges(alloc) {}

    vertex_table_value_type(const vertex_table_value_type &) = default;
    vertex_table_value_type(vertex_table_value_type &&) noexcept = default;

    vertex_table_value_type(const vertex_table_value_type &other, allocator_type alloc)
        : vertex_data(other.vertex_data, alloc),
          edges(other.edges, alloc) {}

    vertex_table_value_type(vertex_table_value_type &&other, allocator_type alloc) noexcept
        : vertex_data(std::move(other.vertex_data), alloc),
          edges(std::move(other.edges), alloc) {}

    vertex_data_type vertex_data;
    edge_table_type edges;
  };
  using vertex_table_type = boost::unordered_map<key_type,
                                                 vertex_table_value_type,
                                                 metall::utility::hash<key_type>,
                                                 std::equal_to<>,
                                                 other_scoped_allocator<std::pair<const key_type,
                                                                                  vertex_table_value_type>>>;

 public:

  /// \brief Vertex iterator over a container of vertex data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using vertex_iterator = jgdtl::vertex_iterator_impl<typename vertex_table_type::iterator>;

  /// \brief Const vertex iterator.
  using const_vertex_iterator = jgdtl::vertex_iterator_impl<typename vertex_table_type::const_iterator>;

  /// \brief Edge iterator over a container of edge data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using edge_iterator = jgdtl::edge_iterator_impl<typename edge_table_type::iterator>;

  /// \brief Const edge iterator.
  using const_edge_iterator = jgdtl::edge_iterator_impl<typename edge_table_type::const_iterator>;

  /// \brief Constructor
  /// \param alloc An allocator object
  explicit jgraph_primitive_id(const allocator_type &alloc = allocator_type())
      : m_vertex_table(alloc) {}

  /// \brief Checks if a vertex exists.
  /// \param vertex_id A vertex ID to check.
  /// \return Returns true if the vertex exists; otherwise, returns false.
  bool has_vertex(const key_type &vertex_id) const {
    return m_vertex_table.count(vertex_id) > 0;
  }

  bool has_edge(const key_type &source_id, const key_type &destination_id) const {
    if (m_vertex_table.count(source_id) == 0) {
      return false;
    }

    return m_vertex_table.at(source_id).edges.count(destination_id);
  }

  /// \brief Registers a vertex.
  /// \param vertex_id A vertex ID.
  /// \return Returns if the vertex is registered.
  /// Returns false if the same ID already exists.
  vertex_iterator add_vertex(const key_type &vertex_id) {
    if (has_vertex(vertex_id)) {
      return find_vertex(vertex_id); // Already exist
    }

    auto pos = m_vertex_table.emplace(vertex_id,
                                      vertex_table_value_type{vertex_id, m_vertex_table.get_allocator()});

    return vertex_iterator{pos.first};
  }

  /// \brief Registers an edge.
  /// If a vertex does not exist, it will be registered automatically.
  /// \param source_id A source vertex ID.
  /// \param destination_id A destination vertex ID.
  /// \param edge_id An edge ID to register.
  /// \return Returns if the edge is registered correctly.
  /// Returns false if an error happens.
  edge_iterator add_edge(key_type source_id, key_type destination_id) {
    add_vertex(source_id);
    add_vertex(destination_id);

    key_value_pair_type kv(destination_id, value_type{m_vertex_table.get_allocator()});
    edge_table_value_type edge_data(std::move(kv));

    auto pos = m_vertex_table.at(source_id).edges.emplace(destination_id, std::move(edge_data));
    return edge_iterator{pos};
  }

  vertex_iterator find_vertex(const key_type &vertex_id) {
    return vertex_iterator(m_vertex_table.find(vertex_id));
  }

  const_vertex_iterator find_vertex(const key_type &vertex_id) const {
    return const_vertex_iterator(m_vertex_table.find(vertex_id));
  }

  edge_iterator find_edge(const key_type &source_id, const key_type &destination_id) {
    return edge_iterator(m_vertex_table.at(source_id).edges.find(destination_id));
  }

  const_edge_iterator find_edge(const key_type &source_id, const key_type &destination_id) const {
    return edge_iterator(m_vertex_table.at(source_id).edges.find(destination_id));
  }

  /// \brief Returns the number of vertices.
  /// \return The number of vertices.
  std::size_t num_vertices() const {
    return m_vertex_table.size();
  }

  /// \brief Returns the number of edges.
  /// \return The number of edges.
  std::size_t num_edges() const {
    assert(1);
  }

  /// \brief Returns the degree of the vertex corresponds to 'vid'.
  /// \param vid A vertex ID.
  /// \return Returns the degree of the vertex corresponds to 'vid'.
  /// If no vertex is associated with 'vid', returns 0.
  std::size_t degree(const key_type &vertex_id) const {
    if (!has_vertex(vertex_id)) return 0;

    return m_vertex_table.at(vertex_id).edges.size();
  }

  vertex_iterator vertices_begin() {
    return vertex_iterator{m_vertex_table.begin()};
  }

  const_vertex_iterator vertices_begin() const {
    return const_vertex_iterator{m_vertex_table.begin()};
  }

  vertex_iterator vertices_end() {
    return vertex_iterator{m_vertex_table.end()};
  }

  const_vertex_iterator vertices_end() const {
    return const_vertex_iterator{m_vertex_table.end()};
  }

  edge_iterator edges_begin(const key_type &vid) {
    add_vertex(vid);
    return edge_iterator{m_vertex_table.at(vid).edges.begin()};
  }

  const_edge_iterator edges_begin(const key_type &vid) const {
    return edge_iterator{m_vertex_table.at(vid).edges.begin()};
  }

  edge_iterator edges_end(const key_type &vid) {
    add_vertex(vid);
    return edge_iterator{m_vertex_table.at(vid).edges.end()};
  }

  const_edge_iterator edges_end(const key_type &vid) const {
    return edge_iterator{m_vertex_table.at(vid).edges.end()};
  }

  allocator_type get_allocator() const {
    return m_vertex_table.get_allocator();
  }

 private:

  vertex_table_type m_vertex_table;
};

namespace jgdtl {

template <typename vertex_table_iterator_type>
class vertex_iterator_impl {
 private:

 public:
  using value_type = typename std::tuple_element<1,
                                                 typename std::iterator_traits<vertex_table_iterator_type>::value_type>::type::vertex_data_type;
  using pointer = typename std::pointer_traits<typename std::iterator_traits<vertex_table_iterator_type>::pointer>::template rebind<
      value_type>;
  using reference = value_type &;
  using difference_type = typename std::iterator_traits<vertex_table_iterator_type>::difference_type;

  explicit vertex_iterator_impl(vertex_table_iterator_type begin_pos)
      : m_current_pos(begin_pos) {}

  vertex_iterator_impl(const vertex_iterator_impl &) = default;
  vertex_iterator_impl(vertex_iterator_impl &&) noexcept = default;

  vertex_iterator_impl &operator=(const vertex_iterator_impl &) = default;
  vertex_iterator_impl &operator=(vertex_iterator_impl &&) noexcept = default;

  vertex_iterator_impl &operator++() {
    ++m_current_pos;
    return *this;
  }

  vertex_iterator_impl operator++(int) {
    vertex_iterator_impl tmp(*this);
    operator++();
    return tmp;
  }

  bool equal(const vertex_iterator_impl &other) const {
    return m_current_pos == other.m_current_pos;
  }

  pointer operator->() {
    return &(m_current_pos->second.vertex_data);
  }

  const pointer operator->() const {
    return &(m_current_pos->second.vertex_data);
  }

  reference operator*() {
    return m_current_pos->second.vertex_data;
  }

  const reference operator*() const {
    return m_current_pos->second.vertex_data;
  }

 private:
  vertex_table_iterator_type m_current_pos;
};

template <typename vertex_table_iterator_type>
inline bool operator==(const vertex_iterator_impl<vertex_table_iterator_type> &lhs,
                       const vertex_iterator_impl<vertex_table_iterator_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename vertex_table_iterator_type>
inline bool operator!=(const vertex_iterator_impl<vertex_table_iterator_type> &lhs,
                       const vertex_iterator_impl<vertex_table_iterator_type> &rhs) {
  return !(lhs == rhs);
}

template <typename edge_table_iterator_type>
class edge_iterator_impl {
 private:

 public:
  using value_type = typename std::tuple_element<1,
                                                 typename std::iterator_traits<edge_table_iterator_type>::value_type>::type::edge_data_type;
  using pointer = typename std::pointer_traits<typename std::iterator_traits<edge_table_iterator_type>::pointer>::template rebind<
      value_type>;
  using reference = value_type &;
  using difference_type = typename std::iterator_traits<edge_table_iterator_type>::difference_type;

  explicit edge_iterator_impl(edge_table_iterator_type begin_pos)
      : m_current_pos(begin_pos) {}

  edge_iterator_impl(const edge_iterator_impl &) = default;
  edge_iterator_impl(edge_iterator_impl &&) noexcept = default;

  edge_iterator_impl &operator=(const edge_iterator_impl &) = default;
  edge_iterator_impl &operator=(edge_iterator_impl &&) noexcept = default;

  edge_iterator_impl &operator++() {
    ++m_current_pos;
    return *this;
  }

  edge_iterator_impl operator++(int) {
    edge_iterator_impl tmp(*this);
    operator++();
    return tmp;
  }

  bool equal(const edge_iterator_impl &other) const {
    return m_current_pos == other.m_current_pos;
  }

  pointer operator->() {
    return &(m_current_pos->second.edge_data);
  }

  const pointer operator->() const {
    return &(m_current_pos->second.edge_data);
  }

  reference operator*() {
    return m_current_pos->second.edge_data;
  }

  const reference operator*() const {
    return m_current_pos->second.edge_data;
  }

 private:
  edge_table_iterator_type m_current_pos;
};

template <typename edge_table_iterator_type>
inline bool operator==(const edge_iterator_impl<edge_table_iterator_type> &lhs,
                       const edge_iterator_impl<edge_table_iterator_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename edge_table_iterator_type>
inline bool operator!=(const edge_iterator_impl<edge_table_iterator_type> &lhs,
                       const edge_iterator_impl<edge_table_iterator_type> &rhs) {
  return !(lhs == rhs);
}

}

} // namespace metall::container::experimental::jgraph

/// \example jgraph.cpp
/// This is an example of how to use the jgraph.


#endif //METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_PRIMITIVE_ID_HPP
