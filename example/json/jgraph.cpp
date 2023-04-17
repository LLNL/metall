// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>
#include <metall/metall.hpp>
#include <metall/container/experimental/jgraph/jgraph.hpp>

using namespace metall::container::experimental;
using graph_type = jgraph::jgraph<metall::manager::allocator_type<std::byte>>;

std::vector<std::string> input_json_string_list = {
    R"({"type":"node", "id":"0", "properties":["user0"]})",
    R"({"type":"node", "id":"1", "properties":["user1"]})",
    R"({"type":"node", "id":"2", "properties":["item0"]})",
    R"({"type":"node", "id":"3", "properties":["item1"]})",
    R"({"type":"relationship", "id":"0", "start":"0", "end":"2", "properties":["buy"]})",
    R"({"type":"relationship", "id":"1", "start":"1", "end":"3", "properties":["sell"]})",
    R"({"type":"relationship", "id":"2", "start":"0", "end":"1", "properties":["friend"]})",
    R"({"type":"relationship", "id":"3", "start":"0", "end":"1", "properties":["customer"]})"};

int main() {
  {
    std::cout << "-- Create ---" << std::endl;
    metall::manager manager(metall::create_only, "./jgraph_obj");

    auto *graph = manager.construct<graph_type>(metall::unique_instance)(
        manager.get_allocator());
    for (const auto &json_string : input_json_string_list) {
      auto value = metall::json::parse(json_string, manager.get_allocator());

      if (value.as_object()["type"].as_string() == "node") {
        const auto &vertex_id = value.as_object()["id"].as_string();
        graph->register_vertex(vertex_id)->value() = std::move(value);
      } else if (value.as_object()["type"].as_string() == "relationship") {
        const auto &src_id = value.as_object()["start"].as_string();
        const auto &dst_id = value.as_object()["end"].as_string();
        graph->register_edge(src_id, dst_id, true)->value() = std::move(value);
      }
    }
    std::cout << "#of vertices: " << graph->num_vertices() << std::endl;
    std::cout << "#of edges: " << graph->num_edges() << std::endl;
  }

  {
    std::cout << "\n--- Open ---" << std::endl;
    metall::manager manager(metall::open_read_only, "./jgraph_obj");

    const auto *const graph =
        manager.find<graph_type>(metall::unique_instance).first;

    std::cout << "<Vertices>" << std::endl;
    metall::json::pretty_print(std::cout, graph->find_vertex("0")->value());
    metall::json::pretty_print(std::cout, graph->find_vertex("1")->value());
    metall::json::pretty_print(std::cout, graph->find_vertex("2")->value());
    metall::json::pretty_print(std::cout, graph->find_vertex("3")->value());

    // Access edge values and edge values using the iterators
    std::cout << "\n<Edges>" << std::endl;
    for (auto vitr = graph->vertices_begin(), vend = graph->vertices_end();
         vitr != vend; ++vitr) {
      for (auto eitr = graph->edges_begin(vitr->id()),
                eend = graph->edges_end(vitr->id());
           eitr != eend; ++eitr) {
        if (vitr->id() == eitr->source_id()) {
          std::cout << "Source vertex ID = " << eitr->source_id() << std::endl;
          std::cout << "Destination vertex ID = " << eitr->destination_id()
                    << std::endl;
        } else {
          std::cout << "Source vertex ID = " << eitr->destination_id()
                    << std::endl;
          std::cout << "Destination vertex ID = " << eitr->source_id()
                    << std::endl;
        }
        metall::json::pretty_print(std::cout, eitr->value());
      }
      std::cout << std::endl;
    }
  }

  return 0;
}
