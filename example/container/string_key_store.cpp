// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>
#include <metall/container/string_key_store.hpp>
#include <metall/container/experimental/json/json.hpp>

namespace mc = metall::container;
namespace mj = mc::experimental::json;

// Example of a string-key-store with int value.
void int_store_example();

// Example of a string-key-store with JSON value.
void json_store_example();

int main() {
  int_store_example();
  json_store_example();
}

void int_store_example() {
  using int_store_type = mc::string_key_store<int>;
  {
    metall::manager manager(metall::create_only, "./string_key_store_obj");

    // Allocate an instance of the int-store which accept duplicate keys by default.
    auto *store = manager.construct<int_store_type>("int-store")(manager.get_allocator());

    store->insert("a"); // Insert with the default value
    store->insert("b", 0); // Insert with a value
    store->insert("b", 1); // Insert another element with an existing key
  }

  {
    metall::manager manager(metall::open_read_only, "./string_key_store_obj");
    auto *store = manager.find<int_store_type>("int-store").first;

    // Iterate over all elements
    for (auto loc = store->begin(); loc != store->end(); ++loc) {
      std::cout << store->key(loc) << " : " << store->value(loc) << std::endl;
    }
  }
}

void json_store_example() {
  using json_type = mj::value<metall::manager::allocator_type<std::byte>>;
  using json_store_type = mc::string_key_store<json_type>;
  {
    metall::manager manager(metall::open_only, "./string_key_store_obj");

    const bool unique = true;
    const uint64_t hash_seed = 123;
    auto *store = manager.construct<json_store_type>("json-store")(unique, hash_seed, manager.get_allocator());

    store->insert("a",
                  mj::parse(R"({"name":"Alice"})", manager.get_allocator()));

    store->insert("b",
                  mj::parse(R"({"name":"N/A"})", manager.get_allocator()));
    store->insert("b",
                  mj::parse(R"({"name":"Bob"})", manager.get_allocator())); // Overwrite
  }

  {
    metall::manager manager(metall::open_read_only, "./string_key_store_obj");
    auto *store = manager.find<json_store_type>("json-store").first;

    // Use find() to locate elements.
    std::cout << "a : " << mj::serialize(store->value(store->find("a"))) << std::endl;
    std::cout << "b : " << mj::serialize(store->value(store->find("b"))) << std::endl;
  }
}
