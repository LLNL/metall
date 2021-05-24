// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <metall/metall.hpp>
#include <metall/container/experiment/jgraph/jgraph.hpp>

using namespace metall::container::experiment;
using graph_type = jgraph::jgraph<metall::manager::allocator_type<std::byte>>;

std::vector<std::string> input_json_string_list = {
    R"({"type":"node", "id":"0", "properties":["user0"]})",
    R"({"type":"node", "id":"1", "properties":["user1"]})",
    R"({"type":"node", "id":"2", "properties":["item0"]})",
    R"({"type":"node", "id":"3", "properties":["item1"]})",
    R"({"id":"100", "type":"relationship", "start":"0", "end":"2", "properties":["buy"]})",
    R"({"id":"101", "type":"relationship", "start":"0", "end":"3", "properties":["buy"]})",
    R"({"id":"102", "type":"relationship", "start":"1", "end":"2", "properties":["buy"]})",
    R"({"id":"103", "type":"relationship", "start":"0", "end":"1", "properties":["friend"]})"
};

int main() {

  {
    std::cout << "-- Create ---" << std::endl;
    metall::manager manager(metall::create_only, "./jgraph_obj");

    auto *graph = manager.construct<graph_type>(metall::unique_instance)(manager.get_allocator());
    for (const auto &json_string : input_json_string_list) {
      auto value = graph_type::value_type(manager.get_allocator());

      json::parse(json_string, &value);

      if (value.as_object()["type"].as_string() == "node") {
        const auto &vertex_id = value.as_object()["id"].as_string();
        graph->vertex_data(vertex_id) = std::move(value);
      } else if (value.as_object()["type"].as_string() == "relationship") {
        const auto &src_id = value.as_object()["start"].as_string();
        const auto &dst_id = value.as_object()["end"].as_string();
        const auto &edge_id = value.as_object()["id"].as_string();
        graph->add_edge(src_id, dst_id, edge_id);
        graph->edge_data(edge_id) = std::move(value);
      }
    }
  }

  {
    std::cout << "\n--- Open ---" << std::endl;
    metall::manager manager(metall::open_read_only, "./jgraph_obj");

    auto *graph = manager.find<graph_type>(metall::unique_instance).first;

    std::cout << "<Vertices>" << std::endl;
    for (auto vitr = graph->vertices_begin(), vend = graph->vertices_end(); vitr != vend; ++vitr) {
      const auto &src_vid = vitr->first;
      std::cout << graph->vertex_data(src_vid) << std::endl;
    }

    std::cout << "\n<Edges>" << std::endl;
    for (auto vitr = graph->vertices_begin(), vend = graph->vertices_end(); vitr != vend; ++vitr) {
      const auto &src_vid = vitr->first;
      const auto &edge_list = vitr->second;
      for (auto eitr = edge_list.begin(), eend = edge_list.end(); eitr != eend; ++eitr) {
        const auto &dst_vid = eitr->first;
        const auto &edge_id = eitr->second;
        std::cout << graph->edge_data(edge_id) << std::endl;
      }
    }
  }

  return 0;
}
