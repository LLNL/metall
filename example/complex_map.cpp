// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/string.hpp>
#include <boost/container/map.hpp>
#include <metall/metall.hpp>

namespace bc = boost::container;

// Utility to generate a new allocator type for 'new_type'
template <typename allocator_type, typename new_type>
using rebind_alloc = typename std::allocator_traits<
    allocator_type>::template rebind_alloc<new_type>;

// Key type
using key_type = int;

// Mapped type
template <typename _allocator_type>
struct mapped_type {
  using allocator_type = _allocator_type;

  bc::vector<int, rebind_alloc<_allocator_type, int>> vec;
  bc::basic_string<char, std::char_traits<char>,
                   rebind_alloc<_allocator_type, char>>
      str;

  explicit mapped_type(const allocator_type &allocator)
      : vec(allocator), str(allocator) {}
};

// Value type
template <typename allocator_type>
using value_type = std::pair<const key_type, mapped_type<allocator_type>>;

// Map type
template <typename base_allocator_type>
using map_allocator_type = bc::scoped_allocator_adaptor<
    rebind_alloc<base_allocator_type, value_type<base_allocator_type>>>;
template <typename base_allocator_type>
using map_type =
    boost::container::map<key_type, mapped_type<base_allocator_type>,
                          std::less<>, map_allocator_type<base_allocator_type>>;

// Map type instantiated to Metall
using metall_map_type = map_type<metall::manager::allocator_type<std::byte>>;

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pmap =
        manager.construct<metall_map_type>("map")(manager.get_allocator<>());

    (*pmap)[0];
    pmap->at(0).vec.push_back(0);
    pmap->at(0).str = "hello, world 0";

    pmap->try_emplace(1);
    pmap->at(1).vec.push_back(1);
    pmap->at(1).str = "hello, world 1";
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pmap = manager.find<metall_map_type>("map").first;

    std::cout << pmap->at(0).vec[0] << std::endl;  // Prints out "0"
    std::cout << pmap->at(0).str << std::endl;  // Prints out "hello, world 0"

    std::cout << pmap->at(1).vec[0] << std::endl;  // Prints out "1"
    std::cout << pmap->at(1).str << std::endl;  // Prints out "hello, world 1"
  }

  return 0;
}