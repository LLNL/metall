// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <boost/container/string.hpp>
#include <boost/container/map.hpp>
#include <metall/metall.hpp>

// String with Metall
using persistent_string =
    boost::container::basic_string<char, std::char_traits<char>,
                                   metall::manager::allocator_type<char>>;
// Example of string-int map
void string_int_map() {
  using value_type = std::pair<const persistent_string,  // Key type
                               int>;                     // Mapped type
  using string_int_map =
      boost::container::map<persistent_string, int, std::less<>,
                            metall::manager::scoped_allocator_type<value_type>>;

  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pmap = manager.construct<string_int_map>("string-int-map")(
        manager.get_allocator<>());

    pmap->insert(
        value_type(persistent_string("zero", manager.get_allocator<>()), 0));

    (*pmap)[persistent_string("one", manager.get_allocator<>())] = 1;
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pmap = manager.find<string_int_map>("string-int-map").first;

    std::cout << pmap->at(persistent_string("zero", manager.get_allocator<>()))
              << std::endl;  // Will print "0"
    std::cout << pmap->at(persistent_string("one", manager.get_allocator<>()))
              << std::endl;  // Will print "1"
  }
}

// Example of int-string map
// This is also considered as an example of a container of containers
void int_string_map() {
  using value_type = std::pair<const int,           // Key type
                               persistent_string>;  // Mapped type

  using map_allocator_type = metall::manager::scoped_allocator_type<value_type>;
  using int_string_map = boost::container::map<int, persistent_string,
                                               std::less<>, map_allocator_type>;

  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pmap = manager.construct<int_string_map>("int-string-map")(
        manager.get_allocator<>());

    pmap->insert(
        value_type(0, persistent_string("zero", manager.get_allocator<>())));

    (*pmap)[1] = persistent_string("one", manager.get_allocator<>());

    // Mapped objects (persistent_string) use an allocator stored in pmap
    // automatically thanks to scoped allocator
    (*pmap)[2].push_back('t');
    (*pmap)[2].push_back('w');
    (*pmap)[2].push_back('o');
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pmap = manager.find<int_string_map>("int-string-map").first;

    std::cout << pmap->at(0) << std::endl;  // Will print "zero"
    std::cout << pmap->at(1) << std::endl;  // Will print "one"
    std::cout << pmap->at(2) << std::endl;  // Will print "two"
  }
}

int main() {
  string_int_map();
  int_string_map();

  return 0;
}