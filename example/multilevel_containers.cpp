// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// -------------------- //
// Example of container of containers (or multi-level containers) with Metall
// Use the following multi-level containers with Metall
// unordered_map<unsigned int,
//               unordered_multimap<uint64_t,
//                                  vector<char>
//               >
// >;
// As Metall has a STL compatible allocator (metall::manager::allocator_type),
// all you have to do is just following the standard way to use custom allocator
// in containers
// -------------------- //

#include <iostream>
#include <functional>
#include <metall/metall.hpp>

// We recommend you to use containers implemented in Boost libraries because
// they fully support the offset pointer
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/vector.hpp>
#include <boost/unordered_map.hpp>

using boost::unordered_map;
using boost::unordered_multimap;
using boost::container::scoped_allocator_adaptor;
using boost::container::vector;

// Inner vector type
using vector_type = vector<char, metall::manager::allocator_type<char>>;

// Inner multimap type
using multimap_type = unordered_multimap<
    uint64_t, vector_type, std::hash<uint64_t>, std::equal_to<uint64_t>,
    metall::manager::allocator_type<std::pair<uint64_t, vector_type>>>;

// To use custom allocator in multi-level containers, you have to use
// scoped_allocator_adaptor in the most outer container so that the inner
// containers obtain their allocator arguments from the outer containers's
// scoped_allocator_adaptor See:
// https://en.cppreference.com/w/cpp/memory/scoped_allocator_adaptor
using map_allocator_type = scoped_allocator_adaptor<
    metall::manager::allocator_type<std::pair<unsigned int, multimap_type>>>;
using map_type =
    unordered_map<unsigned int, multimap_type, std::hash<unsigned int>,
                  std::equal_to<unsigned int>, map_allocator_type>;

int main() {
  {
    // Create a new metall object
    metall::manager manager(metall::create_only, "/tmp/datastore");

    // Construct an object of map_type
    map_type* pmap =
        manager.construct<map_type>("map")(manager.get_allocator<>());

    // Can use the container as usual
    vector_type vec1(manager.get_allocator());
    vec1.push_back('a');
    (*pmap)[30].emplace(20, vec1);

    // Take a snapshot
    manager.snapshot("/tmp/datastore_snapshot");
  }

  {
    // Open the snapshot
    metall::manager manager(metall::open_only, "/tmp/datastore_snapshot");

    // Find already allocated object with name 'map'
    map_type* pmap = manager.find<map_type>("map").first;

    // Use the container as usual
    auto& mmap = (*pmap)[30];
    auto& vec = mmap.find(20)->second;
    std::cout << vec[0] << std::endl;  // Print 'a'
  }

  return 0;
}