// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP
#define METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP

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
}

// --- Forward declarations --- //
template <typename allocator_type>
class jgraph;

namespace jgdtl {
template <typename storage_iterator_type>
class vertex_iterator_impl;

template <typename directory_iterator_type,
          typename storage_pointer_type,
          typename storage_value_type>
class edge_iterator_impl;
} // namespace jgdtl

/// \brief JSON Graph which can be used with Metall.
/// Supported graph type:
/// There is a single 'JSON value' data per vertex and edge
/// Every vertex and edge has an unique ID.
/// \tparam allocator_type Allocator type.
template <typename allocator_type>
class jgraph {
 private:
  template <typename T>
  using other_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<other_allocator<T>>;

  using key_value_pair_type = json::key_value_pair<char, std::char_traits<char>, allocator_type>;
  using vertex_storage_type = bc::vector<key_value_pair_type, other_scoped_allocator<key_value_pair_type>>;
  using edge_storage_type = bc::vector<key_value_pair_type, other_scoped_allocator<key_value_pair_type>>;

  struct dst_vertex_directory_value_type {
    std::size_t dst_vertex_position;
    std::size_t edge_position;
  };

  using dst_vertex_directory_type = boost::unordered_multimap<uint64_t,
                                                              dst_vertex_directory_value_type,
                                                              metall::utility::hash<uint64_t>,
                                                              std::equal_to<>,
                                                              other_scoped_allocator<std::pair<const uint64_t,
                                                                                               dst_vertex_directory_value_type>>>;

  struct vertex_directory_value_type {
    std::size_t vertex_position;
    dst_vertex_directory_type dst_vertex_directory;
  };
  using vertex_directory_type = boost::unordered_multimap<uint64_t,
                                                          vertex_directory_value_type,
                                                          metall::utility::hash<uint64_t>,
                                                          std::equal_to<>,
                                                          other_scoped_allocator<std::pair<const uint64_t,
                                                                                           vertex_directory_value_type>>>;



  struct edge_directory_value_type {
    std::size_t edge_position;
  };

  using edge_directory_type = boost::unordered_multimap<uint64_t,
                                                        edge_directory_value_type,
                                                        metall::utility::hash<uint64_t>,
                                                        std::equal_to<>,
                                                        other_scoped_allocator<std::pair<const uint64_t,
                                                                                         edge_directory_value_type>>>;

 public:
  /// \brief JSON value type every vertex and edge has
  using value_type = typename key_value_pair_type::value_type;

  /// \brief Vertex iterator over a container of vertex data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using vertex_iterator = jgdtl::vertex_iterator_impl<typename vertex_storage_type::iterator>;

  /// \brief Const vertex iterator.
  using const_vertex_iterator = jgdtl::vertex_iterator_impl<typename vertex_storage_type::const_iterator>;

  /// \brief Edge iterator over a container of edge data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using edge_iterator = jgdtl::edge_iterator_impl<typename dst_vertex_directory_type::iterator,
                                                  typename std::pointer_traits<typename std::allocator_traits<
                                                      allocator_type>::pointer>::template rebind<edge_storage_type>,
                                                  typename edge_storage_type::value_type>;
  /// \brief Const edge iterator.
  using const_edge_iterator = jgdtl::edge_iterator_impl<typename dst_vertex_directory_type::const_iterator,
                                                        typename std::pointer_traits<typename std::allocator_traits<
                                                            allocator_type>::const_pointer>::template rebind<
                                                            edge_storage_type>,
                                                        const typename edge_storage_type::value_type>;

  /// \brief Constructor
  /// \param alloc An allocator object
  explicit jgraph(const allocator_type &alloc = allocator_type())
      : m_vertex_storage(alloc),
        m_edge_storage(alloc),
        m_vertex_directory(alloc),
        m_edge_directory(alloc) {}

  /// \brief Checks if a vertex exists.
  /// \param vertex_id A vertex ID to check.
  /// \return Returns true if the vertex exists; otherwise, returns false.
  bool has_vertex(const std::string_view &vertex_id) const {
    return priv_locate_vertex(vertex_id) != m_vertex_directory.end();
  }

  /// \brief Checks if an edge exists.
  /// \param edge_id A edge ID to check.
  /// \return Returns true if the edge exists; otherwise, returns false.
  bool has_edge(const std::string_view &edge_id) const {
    return priv_locate_edge(edge_id) != m_edge_directory.end();
  }

  /// \brief Registers a vertex.
  /// \param vertex_id A vertex ID.
  /// \return Returns if the vertex is registered.
  /// Returns false if the same ID already exists.
  bool register_vertex(const std::string_view &vertex_id) {
    if (has_vertex(vertex_id)) {
      return false; // Already exist
    }

    const auto vertex_pos = priv_add_vertex_storage_item(vertex_id);
    m_vertex_directory.emplace(priv_hash_id(vertex_id),
                               vertex_directory_value_type{vertex_pos,
                                                           dst_vertex_directory_type{
                                                               m_vertex_directory.get_allocator()}});
    return true;
  }

  /// \brief Registers an edge.
  /// If a vertex does not exist, it will be registered automatically.
  /// \param source_id A source vertex ID.
  /// \param destination_id A destination vertex ID.
  /// \param edge_id An edge ID to register.
  /// \return Returns if the edge is registered correctly.
  /// Returns false if an error happens.
  bool register_edge(const std::string_view &source_id, const std::string_view &destination_id,
                     const std::string_view &edge_id) {
    auto edge_itr = priv_locate_edge(edge_id);
    if (edge_itr != m_edge_directory.end()) {
      const auto existing_edge_pos = edge_itr->second.edge_position;
      return priv_add_vertex_directory_item(source_id, destination_id, existing_edge_pos);
    }

    register_vertex(source_id);
    register_vertex(destination_id);

    const auto edge_pos = priv_add_edge_storage_item(edge_id);
    return priv_add_vertex_directory_item(source_id, destination_id, edge_pos)
        && priv_add_edge_directory_item(edge_id, edge_pos);
  }

  /// \brief Returns a reference to the value of the existing vertex whose key is equivalent to vertex_id.
  /// \param vertex_id A vertex ID to find.
  /// \return Returns a reference to the value of the existing vertex whose key is equivalent to vertex_id.
  value_type &vertex_value(const std::string_view &vertex_id) {
    auto vpos = priv_locate_vertex(vertex_id);
    return m_vertex_storage[vpos->second.vertex_position].value();
  }

  /// \brief Const version of vertex_value().
  const value_type &vertex_value(const std::string_view &vertex_id) const {
    const auto vpos = priv_locate_vertex(vertex_id);
    return m_vertex_storage[vpos->second.vertex_position].value();
  }

  /// \brief Returns a reference to the value of the existing edge whose key is equivalent to edge_id.
  /// \param edge_id A edge ID to find.
  /// \return Returns a reference to the value of the existing edge whose key is equivalent to edge_id.
  value_type &edge_value(const std::string_view &edge_id) {
    auto epos = priv_locate_edge(edge_id);
    return m_edge_storage[epos->second.edge_position].value();
  }

  /// \brief Const version of edge_value().
  const value_type &edge_value(const std::string_view &edge_id) const {
    const auto epos = priv_locate_edge(edge_id);
    return m_edge_storage[epos->second.edge_position].value();
  }

  /// \brief Returns the number of vertices.
  /// \return The number of vertices.
  std::size_t num_vertices() const {
    return m_vertex_storage.size();
  }

  /// \brief Returns the number of edges.
  /// \return The number of edges.
  std::size_t num_edges() const {
    return m_edge_storage.size();
  }

  /// \brief Returns the degree of the vertex corresponds to 'vid'.
  /// \param vid A vertex ID.
  /// \return Returns the degree of the vertex corresponds to 'vid'.
  /// If no vertex is associated with 'vid', returns 0.
  std::size_t degree(const std::string_view &vid) const {
    auto pos = priv_locate_vertex(vid);
    if (pos == m_vertex_directory.cend()) {
      return 0;
    }
    return pos->second.dst_vertex_directory.size();
  }

  vertex_iterator vertices_begin() {
    return vertex_iterator(m_vertex_storage.begin());
  }

  const_vertex_iterator vertices_begin() const {
    return const_vertex_iterator(m_vertex_storage.begin());
  }

  vertex_iterator vertices_end() {
    return vertex_iterator(m_vertex_storage.end());
  }

  const_vertex_iterator vertices_end() const {
    return const_vertex_iterator(m_vertex_storage.end());
  }

  edge_iterator edges_begin(const std::string_view &vid) {
    auto pos = priv_locate_vertex(vid);
    return edge_iterator(pos->second.dst_vertex_directory.begin(), &m_edge_storage);
  }

  const_edge_iterator edges_begin(const std::string_view &vid) const {
    const auto pos = priv_locate_vertex(vid);
    return const_edge_iterator(pos->second.dst_vertex_directory.begin(),
                               const_cast<edge_storage_type *>(&m_edge_storage));
  }

  edge_iterator edges_end(const std::string_view &vid) {
    auto pos = priv_locate_vertex(vid);
    return edge_iterator(pos->second.dst_vertex_directory.end(), &m_edge_storage);
  }

  const_edge_iterator edges_end(const std::string_view &vid) const {
    const auto pos = priv_locate_vertex(vid);
    return const_edge_iterator(pos->second.dst_vertex_directory.end(),
                               const_cast<edge_storage_type *>(&m_edge_storage));
  }

  allocator_type get_allocator() const {
    return m_vertex_storage.get_allocator();
  }

 private:

  typename vertex_directory_type::const_iterator priv_locate_vertex(const std::string_view &vid) const {
    const auto hash = priv_hash_id(vid.data());
    auto range = m_vertex_directory.equal_range(hash);
    for (auto itr = range.first, end = range.second; itr != end; ++itr) {
      const vertex_directory_value_type &value = itr->second;
      if (m_vertex_storage[value.vertex_position].key() == vid) {
        return itr; // Found the key
      }
    }
    return m_vertex_directory.end(); // Couldn't find
  }

  typename vertex_directory_type::iterator priv_locate_vertex(const std::string_view &vid) {
    const auto citr = const_cast<const jgraph *>(this)->priv_locate_vertex(vid);
    return m_vertex_directory.erase(citr, citr); // Generate non-const iterator from const iterator
  }

  typename edge_directory_type::const_iterator priv_locate_edge(const std::string_view &edge_id) const {
    const auto hash = priv_hash_id(edge_id);
    auto range = m_edge_directory.equal_range(hash);
    for (auto itr = range.first, end = range.second; itr != end; ++itr) {
      if (m_edge_storage[itr->second.edge_position].key() == edge_id) {
        return itr; // Found the key
      }
    }
    return m_edge_directory.end(); // Couldn't find
  }

  typename edge_directory_type::iterator priv_locate_edge(const std::string_view &edge_id) {
    const auto citr = const_cast<const jgraph *>(this)->priv_locate_edge(edge_id);
    return m_edge_directory.erase(citr, citr); // Generate non-const iterator from const iterator
  }

  static uint64_t priv_hash_id(const std::string_view &id) {
    return metall::mtlldetail::MurmurHash64A(id.data(), id.length(), 1234);
  }

  std::size_t priv_add_edge_storage_item(const std::string_view &edge_id) {
    m_edge_storage.emplace_back(edge_id, typename key_value_pair_type::value_type{m_vertex_storage.get_allocator()});
    return m_edge_storage.size() - 1;
  }

  bool priv_add_edge_directory_item(const std::string_view &edge_id, const std::size_t edge_storage_pos) {
    m_edge_directory.emplace(priv_hash_id(edge_id), edge_directory_value_type{edge_storage_pos});
    return true;
  }

  std::size_t priv_add_vertex_storage_item(const std::string_view &vertex_id) {
    m_vertex_storage.emplace_back(vertex_id,
                                  typename key_value_pair_type::value_type{m_vertex_storage.get_allocator()});
    return m_vertex_storage.size() - 1;
  }

  bool priv_add_vertex_directory_item(const std::string_view &source_id, const std::string_view &destination_id,
                                      const std::size_t edge_storage_position) {
    vertex_directory_value_type &src_val = priv_locate_vertex(source_id)->second;
    vertex_directory_value_type &dst_val = priv_locate_vertex(destination_id)->second;
    src_val.dst_vertex_directory.emplace(priv_hash_id(destination_id),
                                         dst_vertex_directory_value_type{dst_val.vertex_position,
                                                                         edge_storage_position});
    return true;
  }

  vertex_storage_type m_vertex_storage;
  edge_storage_type m_edge_storage;
  vertex_directory_type m_vertex_directory;
  edge_directory_type m_edge_directory;
};

namespace jgdtl {

template <typename storage_iterator_type>
class vertex_iterator_impl {
 public:

  using value_type = typename std::iterator_traits<storage_iterator_type>::value_type;
  using pointer = typename std::iterator_traits<storage_iterator_type>::pointer;
  using reference = typename std::iterator_traits<storage_iterator_type>::reference;
  using difference_type = typename std::iterator_traits<storage_iterator_type>::difference_type;

  explicit vertex_iterator_impl(storage_iterator_type begin_pos)
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
    return &(*m_current_pos);
  }

  const pointer operator->() const {
    return &(*m_current_pos);
  }

  reference operator*() {
    return *m_current_pos;
  }

  const reference operator*() const {
    return *m_current_pos;
  }

 private:
  storage_iterator_type m_current_pos;
};

template <typename storage_iterator_type>
inline bool operator==(const vertex_iterator_impl<storage_iterator_type> &lhs,
                       const vertex_iterator_impl<storage_iterator_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename storage_iterator_type>
inline bool operator!=(const vertex_iterator_impl<storage_iterator_type> &lhs,
                       const vertex_iterator_impl<storage_iterator_type> &rhs) {
  return !(lhs == rhs);
}

template <typename directory_iterator_type,
          typename storage_pointer_type,
          typename storage_value_type>
class edge_iterator_impl {
 public:

  using value_type = storage_value_type;
  using pointer = value_type *;
  using reference = value_type &;
  using difference_type = typename std::iterator_traits<directory_iterator_type>::difference_type;

  edge_iterator_impl(directory_iterator_type begin_pos, storage_pointer_type storage)
      : m_current_pos(begin_pos),
        m_storage_pointer(storage) {}

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
    return &((*m_storage_pointer)[m_current_pos->second.edge_position]);
  }

  const pointer operator->() const {
    return &((*m_storage_pointer)[m_current_pos->second.edge_position]);
  }

  reference operator*() {
    return (*m_storage_pointer)[m_current_pos->second.edge_position];
  }

  const reference operator*() const {
    return (*m_storage_pointer)[m_current_pos->second.edge_position];
  }

 private:
  directory_iterator_type m_current_pos;
  storage_pointer_type m_storage_pointer;
};

template <typename directory_iterator_type,
          typename storage_pointer_type,
          typename storage_value_type>
inline bool operator==(const edge_iterator_impl<directory_iterator_type, storage_pointer_type, storage_value_type> &lhs,
                       const edge_iterator_impl<directory_iterator_type,
                                                storage_pointer_type,
                                                storage_value_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename directory_iterator_type,
          typename storage_pointer_type,
          typename storage_value_type>
inline bool operator!=(const edge_iterator_impl<directory_iterator_type, storage_pointer_type, storage_value_type> &lhs,
                       const edge_iterator_impl<directory_iterator_type,
                                                storage_pointer_type,
                                                storage_value_type> &rhs) {
  return !(lhs == rhs);
}

}

} // namespace metall::container::experimental::jgraph

/// \example jgraph.cpp
/// This is an example of how to use the jgraph.

#endif //METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP
