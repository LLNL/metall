// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/metall.hpp>
#include <metall/container/experimental/json/json.hpp>

using namespace metall::container::experimental;

int main() {

  std::string json_string = R"(
      {
        "pi": 3.141,
        "happy": true,
        "name": "Niels",
        "nothing": null,
        "answer": {
          "everything": 42
        },
        "list": [1, 0, 2],
        "object": {
          "currency": "USD",
          "value": 42.99
        }
      }
    )";

  std::cout << "Create" << std::endl;

  using metall_value_type = json::value<metall::manager::allocator_type<std::byte>>;
  metall::manager manager(metall::create_only, "./test");

  auto *value = manager.construct<metall_value_type>(metall::unique_instance)(json_string, manager.get_allocator());
  std::cout << *value << std::endl;

  return 0;
}
