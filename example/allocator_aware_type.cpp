// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This example shows how to create an allocator-aware type, which takes an
// allocator and can be stored inside an STL container.

#include <memory>
#include <scoped_allocator>

#include <metall/container/vector.hpp>
#include <metall/container/string.hpp>

#include <metall/metall.hpp>

// A simple allocator-aware class, which uses an allocator to allocate internal
// contents and can be stored in an STL container.
//
// This class holds a string (i.e., STL container), which requires an allocator.
// 'Alloc' can be any allocator type compatible with the STL allocator
// requirement.
//
// This class is implemented just following STL's standards — no special
// requirements by Metall. We use Metall container for convenience but another
// string container, such as boost::container::string, is okay.
template <typename Alloc>
struct key_value_pair {
 public:
  // First key point of this example
  // Required to be an allocator-aware type
  using allocator_type = Alloc;

  // Use the given allocator type in the key
  using key_type = metall::container::basic_string<
      char, std::char_traits<char>,
      typename std::allocator_traits<allocator_type>::template rebind_alloc<
          char>>;

  // Default constructor
  explicit key_value_pair(const allocator_type &alloc = allocator_type())
      : key(alloc) {}

  // Copy and move constructors
  key_value_pair(const key_value_pair &) = default;
  key_value_pair(key_value_pair &&) = default;

  // Here is another key point of this example.
  // Allocator-extended copy and move constructors
  key_value_pair(const key_value_pair &other, const allocator_type &alloc)
      : key(other.key, alloc) {}
  key_value_pair(key_value_pair &&other, const allocator_type &alloc)
      : key(std::move(other.key), alloc) {}

  // Assignment operators.
  // Not required to be an allocator-aware type, but we implement them for
  // the purpose of this example.
  key_value_pair &operator=(const key_value_pair &) = default;
  key_value_pair &operator=(key_value_pair &&) = default;

  key_type key;
  int value;
};

// We use Metall in this example but any STL compatible allocator can be used.
template <typename T>
using alloc_t = metall::manager::allocator_type<T>;

using kv_t = key_value_pair<alloc_t<char>>;

// A vector of kv_t objects.
// Use scoped_allocator_adaptor to wrap up an actual allocator type so that
// the inner containers (allocator-aware types) use the same allocator. See:
// https://en.cppreference.com/w/cpp/memory/scoped_allocator_adaptor
using vec_t =
    metall::container::vector<kv_t,
                              std::scoped_allocator_adaptor<alloc_t<kv_t>>>;

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/metall-dir");
    auto *vec = manager.construct<vec_t>("vec")(manager.get_allocator());

    vec->resize(2);

    vec->at(0).key = "key0";
    vec->at(0).value = 10;

    vec->at(1).key = "key1";
    vec->at(1).value = 100;
  }

  {
    metall::manager manager(metall::open_read_only, "/tmp/metall-dir");
    auto *vec = manager.find<vec_t>("vec").first;
    for (const auto &e : *vec) {
      std::cout << e.key << " : " << e.value << std::endl;
    }
  }

  return 0;
}