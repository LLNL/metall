// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <string>

#include <metall/metall.hpp>
#include <metall/container/experimental/string_container/deque.hpp>
#include <metall/container/experimental/string_container/map_from_string.hpp>
#include <metall/container/experimental/string_container/map_to_string.hpp>

using namespace metall::container::experimental::string_container;

int main() {
  // Create a new datastore
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");

    // -----------------------------------------------------
    // Main string table
    auto *main_table = manager.construct<string_table<>>(
        metall::unique_instance)(manager.get_allocator<>());

    // -----------------------------------------------------
    // map from string (map<string, int>)
    auto *map_from_str_1 =
        manager.construct<map_from_string<int>>("map1")(main_table);

    // map operations
    (*map_from_str_1)["hello"] = 0;
    (*map_from_str_1)["world"] = 1;

    // another map that shares the same string table
    auto *map_from_str_2 =
        manager.construct<map_from_string<int>>("map2")(main_table);

    (*map_from_str_2)[std::string("hello")] = 10;
    (*map_from_str_2)["universe"] = 11;

    // -----------------------------------------------------
    // map to string (map<int, string>)
    auto *map_to_str =
        manager.construct<map_to_string<int>>("map3")(main_table);

    (*map_to_str)[0] = "hello";
    (*map_to_str)[1] = "virtual world";

    // -----------------------------------------------------
    // Deque
    auto *dq = manager.construct<deque<>>("dq")(main_table);
    dq->push_back("hello");
    dq->resize(2);  // 2nd element is empty
  }

  // open the existing containers
  {
    metall::manager manager(metall::open_read_only, "/tmp/datastore");

    auto *main_table =
        manager.find<string_table<>>(metall::unique_instance).first;
    auto *map_from_str_1 = manager.find<map_from_string<int>>("map1").first;
    auto *map_from_str_2 = manager.find<map_from_string<int>>("map2").first;
    auto *map_to_str = manager.find<map_to_string<int>>("map3").first;
    auto *dq = manager.find<deque<>>("dq").first;

    std::cout << "\nmap_from_str_1" << std::endl;
    std::cout << "hello: " << (*map_from_str_1)["hello"] << std::endl;
    std::cout << "world: " << (*map_from_str_1)["world"] << std::endl;

    std::cout << "\nmap_from_str_2" << std::endl;
    for (const auto &[k, v] : *map_from_str_2) {
      std::cout << k << ": " << v << std::endl;
    }

    std::cout << "\nmap_to_str" << std::endl;
    for (const auto &[k, v] : *map_to_str) {
      std::cout << k << ": " << v << std::endl;
    }

    std::cout << "\ndeque" << std::endl;
    for (std::size_t i = 0; i < dq->size(); ++i) {
      std::cout << i << ": " << (*dq)[i] << std::endl;
    }

    // NOTE: empty string ("") is also stored in the string table
    std::cout << "\n#of unique strings: " << main_table->size() << std::endl;
  }

  return 0;
}