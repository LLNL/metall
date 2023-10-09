// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP
#define METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP

#include <iostream>
#include <string_view>
#include <functional>
#include <utility>
#include <optional>

#include <metall/container/unordered_map.hpp>
#include <metall/container/vector.hpp>
#include <metall/container/scoped_allocator.hpp>
#include <metall/utility/hash.hpp>
#include <metall/json/json.hpp>

/// \namespace metall::container::experimental
/// \brief Namespace for Metall containers in an experimental phase.
namespace metall::container::experimental {}

/// \namespace metall::container::experimental::jgraph
/// \brief Namespace for Metall JSON graph container, which is in an
/// experimental phase.
namespace metall::container::experimental::jgraph {

namespace {
namespace mc = metall::container;
namespace mj = metall::json;
}  // namespace

// --- Forward declarations --- //
template <typename allocator_type>
class jgraph;

namespace jgdtl {
template <typename storage_iterator_type>
class vertex_iterator_impl;

template <typename adj_list_edge_list_iterator_type,
          typename storage_pointer_type>
class edge_iterator_impl;
}  // namespace jgdtl

template <typename _allocator_type = std::allocator<std::byte>>
class jgraph {
 public:
  using allocator_type = _allocator_type;

  /// \brief The type of vertex ID and edge ID.
  using id_type = std::string_view;

  /// \brief JSON value type every vertex and edge has,
  using value_type = mj::value<allocator_type>;

 private:
  template <typename T>
  using other_allocator =
      typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator =
      mc::scoped_allocator_adaptor<other_allocator<T>>;

  using string_type =
      mc::basic_string<char, std::char_traits<char>,
                       typename std::allocator_traits<
                           allocator_type>::template rebind_alloc<char>>;

  using internal_id_type = uint64_t;
  static constexpr internal_id_type k_max_internal_id =
      std::numeric_limits<internal_id_type>::max();

  class vertex_data_type {
   public:
    using allocator_type = _allocator_type;

    explicit vertex_data_type(
        const id_type &id, const allocator_type &allocator = allocator_type())
        : m_id(id, allocator), m_value(allocator) {}

    /// \brief Copy constructor
    vertex_data_type(const vertex_data_type &) = default;

    /// \brief Allocator-extended copy constructor
    vertex_data_type(const vertex_data_type &other, const allocator_type &alloc)
        : m_id(other.m_id, alloc), m_value(other.m_value, alloc) {}

    /// \brief Move constructor
    vertex_data_type(vertex_data_type &&) noexcept = default;

    /// \brief Allocator-extended move constructor

    vertex_data_type(vertex_data_type &&other,
                     const allocator_type &alloc) noexcept
        : m_id(std::move(other.m_id), alloc),
          m_value(std::move(other.m_value), alloc) {}

    /// \brief Copy assignment operator
    vertex_data_type &operator=(const vertex_data_type &) = default;

    /// \brief Move assignment operator
    vertex_data_type &operator=(vertex_data_type &&) noexcept = default;

    const id_type id() const { return id_type(m_id); }

    value_type &value() { return m_value; }

    const value_type &value() const { return m_value; }

   private:
    string_type m_id;
    value_type m_value;
  };

  using vertex_storage_type =
      mc::unordered_map<internal_id_type, vertex_data_type,
                        metall::utility::hash<>, std::equal_to<>,
                        other_scoped_allocator<std::pair<const internal_id_type,
                                                         vertex_data_type>>>;

  class edge_data_type {
   public:
    using allocator_type = _allocator_type;

    explicit edge_data_type(const id_type &source_id,
                            const id_type &destination_id,
                            const internal_id_type &edge_id,
                            const allocator_type &allocator = allocator_type())
        : m_source_id(source_id, allocator),
          m_destination_id(destination_id, allocator),
          m_edge_id(edge_id),
          m_value(allocator) {}

    /// \brief Copy constructor
    edge_data_type(const edge_data_type &) = default;

    /// \brief Allocator-extended copy constructor

    edge_data_type(const edge_data_type &other, const allocator_type &alloc)
        : m_source_id(other.m_source_id, alloc),
          m_destination_id(other.m_destination_id, alloc),
          m_edge_id(other.m_edge_id),
          m_value(other.m_value, alloc) {}

    /// \brief Move constructor
    edge_data_type(edge_data_type &&) noexcept = default;

    /// \brief Allocator-extended move constructor

    edge_data_type(edge_data_type &&other, const allocator_type &alloc) noexcept
        : m_source_id(other.m_source_id, alloc),
          m_destination_id(other.m_destination_id, alloc),
          m_edge_id(other.m_edge_id),
          m_value(other.m_value, alloc) {}

    /// \brief Copy assignment operator
    edge_data_type &operator=(const edge_data_type &) = default;

    /// \brief Move assignment operator
    edge_data_type &operator=(edge_data_type &&) noexcept = default;

    const id_type source_id() const { return id_type(m_source_id); }

    const id_type destination_id() const { return id_type(m_destination_id); }

    //    const id_type edge_id() const {
    //      return id_type(m_edge_id);
    //    }

    value_type &value() { return m_value; }

    const value_type &value() const { return m_value; }

   private:
    string_type m_source_id;
    string_type m_destination_id;
    internal_id_type m_edge_id;
    value_type m_value;
  };

  using edge_storage_type =
      mc::unordered_map<internal_id_type, edge_data_type,
                        metall::utility::hash<>, std::equal_to<>,
                        other_scoped_allocator<
                            std::pair<const internal_id_type, edge_data_type>>>;

  // Adj-list
  using adj_list_edge_list_type =
      mc::unordered_multimap<internal_id_type,  // hashed destination vid
                             internal_id_type,  // edge id
                             std::hash<internal_id_type>, std::equal_to<>,
                             other_scoped_allocator<std::pair<
                                 const internal_id_type, internal_id_type>>>;

  using adj_list_type = mc::unordered_map<
      internal_id_type,  // hashed source vid
      adj_list_edge_list_type, std::hash<internal_id_type>, std::equal_to<>,
      other_scoped_allocator<
          std::pair<const internal_id_type, adj_list_edge_list_type>>>;

  // ID tables
  using id_table_type = mc::unordered_map<
      internal_id_type, string_type, std::hash<internal_id_type>,
      std::equal_to<>,
      other_scoped_allocator<std::pair<const internal_id_type, string_type>>>;

 public:
  /// \brief Vertex iterator over a container of vertex data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using vertex_iterator =
      jgdtl::vertex_iterator_impl<typename vertex_storage_type::iterator>;

  /// \brief Const vertex iterator.
  using const_vertex_iterator =
      jgdtl::vertex_iterator_impl<typename vertex_storage_type::const_iterator>;

  /// \brief Edge iterator over a container of edge data,
  /// which is metall::container::experimental::json::key_value_pair_type.
  using edge_iterator = jgdtl::edge_iterator_impl<
      typename adj_list_edge_list_type::iterator,
      typename std::pointer_traits<typename std::allocator_traits<
          allocator_type>::pointer>::template rebind<edge_storage_type>>;
  /// \brief Const edge iterator.
  using const_edge_iterator = jgdtl::edge_iterator_impl<
      typename adj_list_edge_list_type::const_iterator,
      typename std::pointer_traits<typename std::allocator_traits<
          allocator_type>::pointer>::template rebind<const edge_storage_type>>;

  /// \brief Constructor
  /// \param alloc An allocator object
  explicit jgraph(const allocator_type &alloc = allocator_type())
      : m_vertex_storage(alloc),
        m_edge_storage(alloc),
        m_adj_list(alloc),
        m_vertex_id_table(alloc) {}

  /// \brief Checks if a vertex exists.
  /// \param vertex_id A vertex ID to check.
  /// \return Returns true if the vertex exists; otherwise, returns false.
  bool has_vertex(const id_type &vertex_id) const {
    return priv_get_vertex_internal_id(vertex_id) != k_max_internal_id;
  }

  std::size_t has_edges(const id_type &source_vertex_id,
                        const id_type &destination_vertex_id) const {
    const auto src = priv_get_vertex_internal_id(source_vertex_id);
    if (src == k_max_internal_id) return 0;

    const auto dst = priv_get_vertex_internal_id(destination_vertex_id);
    if (dst == k_max_internal_id) return 0;

    const auto &edge_list = m_adj_list.at(src);
    return edge_list.count(dst);
  }

  vertex_iterator register_vertex(const id_type &vertex_id) {
    auto internal_id = priv_get_vertex_internal_id(vertex_id);
    if (internal_id != k_max_internal_id) {
      return vertex_iterator(m_vertex_storage.find(internal_id));
    }

    internal_id = priv_generate_vertex_internal_id(vertex_id);

    m_adj_list.emplace(internal_id,
                       adj_list_edge_list_type{m_adj_list.get_allocator()});

    auto ret = m_vertex_storage.emplace(
        internal_id,
        vertex_data_type{vertex_id, m_vertex_storage.get_allocator()});
    return vertex_iterator(ret.first);
  }

  edge_iterator register_edge(const id_type &source_vertex_id,
                              const id_type &destination_vertex_id,
                              const bool undirected = false) {
    register_vertex(source_vertex_id);
    register_vertex(destination_vertex_id);

    const auto src_internal_id = priv_get_vertex_internal_id(source_vertex_id);
    assert(src_internal_id != k_max_internal_id);
    const auto dst_internal_id =
        priv_get_vertex_internal_id(destination_vertex_id);
    assert(dst_internal_id != k_max_internal_id);
    const auto edge_id = priv_generate_edge_id();

    auto itr = m_adj_list.at(src_internal_id).emplace(dst_internal_id, edge_id);
    if (undirected) {
      m_adj_list.at(dst_internal_id).emplace(src_internal_id, edge_id);
    }

    m_edge_storage.emplace(
        edge_id, edge_data_type{source_vertex_id, destination_vertex_id,
                                edge_id, m_edge_storage.get_allocator()});
    return edge_iterator(itr, &m_edge_storage);
  }

  vertex_iterator find_vertex(const id_type &vertex_id) {
    return vertex_iterator(
        m_vertex_storage.find(priv_get_vertex_internal_id(vertex_id)));
  }

  const_vertex_iterator find_vertex(const id_type &vertex_id) const {
    return const_vertex_iterator(
        m_vertex_storage.find(priv_get_vertex_internal_id(vertex_id)));
  }

  std::pair<edge_iterator, edge_iterator> find_edges(
      const id_type &source_vertex_id, const id_type &destination_vertex_id) {
    const auto src_internal_id = priv_get_vertex_internal_id(source_vertex_id);
    const auto dst_internal_id =
        priv_get_vertex_internal_id(destination_vertex_id);

    if (src_internal_id == k_max_internal_id ||
        dst_internal_id == k_max_internal_id) {
      return std::make_pair(edge_iterator{}, edge_iterator{});
    }

    auto &edge_list = m_adj_list.at(src_internal_id);
    auto range = edge_list.equal_range(dst_internal_id);
    return std::make_pair(edge_iterator{range.first, &m_edge_storage},
                          edge_iterator{range.second, &m_edge_storage});
  }

  /// \brief Returns the number of vertices.
  /// \return The number of vertices.
  std::size_t num_vertices() const { return m_vertex_storage.size(); }

  /// \brief Returns the number of edges.
  /// \return The number of edges.
  std::size_t num_edges() const { return m_edge_storage.size(); }

  /// \brief Returns the degree of the vertex corresponds to 'vid'.
  /// \param vertex_id A vertex ID.
  /// \return Returns the degree of the vertex corresponds to 'vid'.
  /// If no vertex is associated with 'vid', returns 0.
  std::size_t degree(const id_type &vertex_id) const {
    const auto internal_id = priv_get_vertex_internal_id(vertex_id);
    if (internal_id == k_max_internal_id) {
      return 0;
    }

    return m_adj_list.at(internal_id).size();
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

  edge_iterator edges_begin(const id_type &vid) {
    const auto internal_id = priv_get_vertex_internal_id(vid);
    if (internal_id == k_max_internal_id) {
      return edge_iterator();
    }
    return edge_iterator{m_adj_list.at(internal_id).begin(), &m_edge_storage};
  }

  const_edge_iterator edges_begin(const id_type &vid) const {
    const auto internal_id = priv_get_vertex_internal_id(vid);
    if (internal_id == k_max_internal_id) {
      return const_edge_iterator();
    }
    return const_edge_iterator{m_adj_list.at(internal_id).begin(),
                               &m_edge_storage};
  }

  edge_iterator edges_end(const id_type &vid) {
    const auto internal_id = priv_get_vertex_internal_id(vid);
    if (internal_id == k_max_internal_id) {
      return edge_iterator();
    }
    return edge_iterator{m_adj_list.at(internal_id).end(), &m_edge_storage};
  }

  const_edge_iterator edges_end(const id_type &vid) const {
    const auto internal_id = priv_get_vertex_internal_id(vid);
    if (internal_id == k_max_internal_id) {
      return const_edge_iterator();
    }
    return const_edge_iterator{m_adj_list.at(internal_id).end(),
                               &m_edge_storage};
  }

  allocator_type get_allocator() const {
    return m_vertex_storage.get_allocator();
  }

 private:
  internal_id_type priv_get_vertex_internal_id(
      const std::string_view &vid) const {
    auto hash = priv_hash_id(vid.data());

    for (std::size_t d = 0; d <= m_max_vid_distance; ++d) {
      const auto vitr = m_vertex_id_table.find(hash);
      if (vitr == m_vertex_id_table.end()) {
        break;
      }

      if (vitr->second == vid) {
        return hash;
      }
      hash = (hash + 1) % k_max_internal_id;
    }

    return k_max_internal_id;  // Couldn't find
  }

  internal_id_type priv_generate_vertex_internal_id(
      const std::string_view &vid) {
    auto hash = priv_hash_id(vid.data());

    std::size_t distance = 0;
    while (m_vertex_id_table.count(hash) > 0) {
      hash = (hash + 1) % k_max_internal_id;
      ++distance;
    }
    m_max_vid_distance = std::max(distance, m_max_vid_distance);

    m_vertex_id_table[hash] = vid.data();

    return hash;
  }

  internal_id_type priv_generate_edge_id() { return ++m_max_edge_id; }

  static internal_id_type priv_hash_id(const std::string_view &id) {
    return metall::mtlldetail::murmur_hash_64a(id.data(), id.length(), 1234);
  }

  vertex_storage_type m_vertex_storage;
  edge_storage_type m_edge_storage;
  adj_list_type m_adj_list;
  id_table_type m_vertex_id_table;
  internal_id_type m_max_edge_id{0};
  std::size_t m_max_vid_distance{0};
};

namespace jgdtl {

template <typename storage_iterator_type>
class vertex_iterator_impl {
 private:
  static constexpr bool is_const_value = std::is_const_v<
      typename std::iterator_traits<storage_iterator_type>::value_type>;

  // Memo: this type will be always non-const even element_type is const
  using raw_value_type = typename std::tuple_element<
      1,
      typename std::iterator_traits<storage_iterator_type>::value_type>::type;

 public:
  using value_type =
      std::conditional_t<is_const_value, const raw_value_type, raw_value_type>;
  using pointer = typename std::pointer_traits<typename std::iterator_traits<
      storage_iterator_type>::pointer>::template rebind<value_type>;
  using reference = value_type &;
  using difference_type =
      typename std::iterator_traits<storage_iterator_type>::difference_type;

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

  pointer operator->() { return const_cast<pointer>(&(m_current_pos->second)); }

  const pointer operator->() const { return &(m_current_pos->second); }

  reference operator*() { return m_current_pos->second; }

  const reference operator*() const { return m_current_pos->second; }

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

template <typename adj_list_edge_list_iterator_type,
          typename storage_pointer_type>
class edge_iterator_impl {
 private:
  static constexpr bool is_const_value = std::is_const_v<
      typename std::pointer_traits<storage_pointer_type>::element_type>;

  // Memo: this type will be always non-const even element_type is const
  using raw_value_type = typename std::pointer_traits<
      storage_pointer_type>::element_type::mapped_type;

 public:
  using value_type =
      std::conditional_t<is_const_value, const raw_value_type, raw_value_type>;
  using pointer = typename std::pointer_traits<
      typename std::pointer_traits<storage_pointer_type>::element_type::
          iterator::pointer>::template rebind<value_type>;
  using reference = value_type &;
  using difference_type = typename std::iterator_traits<
      adj_list_edge_list_iterator_type>::difference_type;

  edge_iterator_impl() : m_current_pos(), m_storage_pointer(nullptr) {}

  edge_iterator_impl(adj_list_edge_list_iterator_type begin_pos,
                     storage_pointer_type storage)
      : m_current_pos(begin_pos), m_storage_pointer(storage) {}

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
    const auto &edge_id = m_current_pos->second;
    return &(m_storage_pointer->at(edge_id));
  }

  const pointer operator->() const {
    const auto &edge_id = m_current_pos->second;
    return &(m_storage_pointer->at(edge_id));
  }

  reference operator*() {
    const auto &edge_id = m_current_pos->second;
    return (m_storage_pointer->at(edge_id));
  }

  const reference operator*() const {
    const auto &edge_id = m_current_pos->second;
    return (m_storage_pointer->at(edge_id));
  }

 private:
  adj_list_edge_list_iterator_type m_current_pos;
  storage_pointer_type m_storage_pointer;
};

template <typename adj_list_edge_list_iterator_type,
          typename storage_pointer_type>
inline bool operator==(
    const edge_iterator_impl<adj_list_edge_list_iterator_type,
                             storage_pointer_type> &lhs,
    const edge_iterator_impl<adj_list_edge_list_iterator_type,
                             storage_pointer_type> &rhs) {
  return lhs.equal(rhs);
}

template <typename adj_list_edge_list_iterator_type,
          typename storage_pointer_type>
inline bool operator!=(
    const edge_iterator_impl<adj_list_edge_list_iterator_type,
                             storage_pointer_type> &lhs,
    const edge_iterator_impl<adj_list_edge_list_iterator_type,
                             storage_pointer_type> &rhs) {
  return !(lhs == rhs);
}

}  // namespace jgdtl

}  // namespace metall::container::experimental::jgraph

/// \example jgraph.cpp
/// This is an example of how to use the jgraph.

#endif  // METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP
