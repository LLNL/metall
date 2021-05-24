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

#include <boost/unordered_map.hpp>

#include <metall/utility/hash.hpp>
#include <metall/container/experiment/json/json.hpp>

namespace metall::container::experiment::jgraph {

namespace {
namespace bc = boost::container;
}

template <typename allocator_type>
class jgraph {
 private:
  template <typename T>
  using other_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator = bc::scoped_allocator_adaptor<other_allocator<T>>;

 public:
  using value_type = json::value<other_allocator<std::byte>>;
  using key_type = typename value_type::string_type;

 private:
  using vertex_data_table_type = json::object<allocator_type>;
  using edge_data_table_type = json::object<allocator_type>;

  using edge_list_type = boost::unordered_multimap<key_type,
                                                   key_type,
                                                   metall::utility::string_hash<key_type>,
                                                   std::equal_to<>,
                                                   other_scoped_allocator<std::pair<const key_type, key_type>>>;
  using adj_list_type = boost::unordered_map<key_type,
                                             edge_list_type,
                                             metall::utility::string_hash<key_type>,
                                             std::equal_to<>,
                                             other_scoped_allocator<std::pair<const key_type, edge_list_type>>>;

 public:
  explicit jgraph(const allocator_type &alloc = allocator_type())
      : m_vertex_data_table(alloc),
        m_edge_data_table(alloc),
        m_adj_list(alloc) {}

  auto &vertex_data(const std::string_view &vertex_id) {
    return m_vertex_data_table[vertex_id];
  }

  const auto &vertex_data(const std::string_view &vertex_id) const {
    return m_vertex_data_table[vertex_id];
  }

  auto &edge_data(const std::string_view &edge_id) {
    return m_edge_data_table[edge_id];
  }

  const auto &edge_data(const std::string_view &edge_id) const {
    return m_edge_data_table[edge_id];
  }

  bool add_edge(const std::string_view &source_vertex_id, const std::string_view &destination_vertex_id,
                const std::string_view &edge_id) {
    auto tmp_source_vertex_id = key_type{source_vertex_id.data(), m_adj_list.get_allocator()};
    auto tmp_destination_vertex_id = key_type{destination_vertex_id.data(), m_adj_list.get_allocator()};
    auto tmp_edge_id = key_type{edge_id.data(), m_adj_list.get_allocator()};

    auto ret1 = m_adj_list.try_emplace(std::move(tmp_source_vertex_id), edge_list_type{m_adj_list.get_allocator()});
    auto &edge_list = ret1.first->second;
    edge_list.emplace(std::move(tmp_destination_vertex_id), std::move(tmp_edge_id));
    return true;
  }

  auto vertices_begin() {
    return m_adj_list.begin();
  }

  const auto vertices_begin() const {
    return m_adj_list.begin();
  }

  auto vertices_end() {
    return m_adj_list.end();
  }

  const auto vertices_end() const {
    return m_adj_list.end();
  }

 private:
  vertex_data_table_type m_vertex_data_table;
  edge_data_table_type m_edge_data_table;
  adj_list_type m_adj_list;
};

} // namespace metall::container::experiment::jgraph

#endif //METALL_CONTAINER_EXPERIMENT_JGRAPH_JGRAPH_HPP
