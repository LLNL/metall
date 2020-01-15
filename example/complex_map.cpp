// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/string.hpp>
#include <boost/container/map.hpp>
#include <metall/metall.hpp>

namespace bc = boost::container;

// Key type
using key_type = int;

// Mapped type
struct mapped_type {
  bc::vector<int, metall::manager::allocator_type<int>> vec;
  bc::basic_string<char, std::char_traits<char>, metall::manager::allocator_type<char>> str;

  template <typename allocator_type>
  explicit mapped_type(const allocator_type &allocator)
      : vec(allocator),
        str(allocator) {}
};

// Value type
using value_type = std::pair<const key_type, mapped_type>;

// Map type
// scoped_allocator_adaptor must be used here as mapped_type also uses a custom allocator
using map_type = boost::container::map<key_type, mapped_type, std::less<>,
                                       bc::scoped_allocator_adaptor<metall::manager::allocator_type<value_type>>>;

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pmap = manager.construct<map_type>("map")(manager.get_allocator<>());

    pmap->emplace(0, mapped_type(manager.get_allocator<>()));
    pmap->at(0).vec.push_back(1);
    pmap->at(0).str = "hello, world";
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pmap = manager.find<map_type>("map").first;
    std::cout << pmap->at(0).vec[0] << std::endl; // Prints out "1"
    std::cout << pmap->at(0).str << std::endl; // Prints out "hello, world"
  }

  return 0;
}