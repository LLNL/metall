// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <boost/container/vector.hpp>

#include <metall/manager.hpp> // Only one header file is required to be included

// Type alias
// This is a standard way to give a custom allocator to a container
using vector_t = boost::container::vector<int, metall::manager::allocator_type<int>>;

int main() {

  {
    // Construct a manager object
    // The current version assumes that there is only one manager object per process
    metall::manager manager(metall::create_only,  // Create a new one
                             "/tmp/file_path",    // The prefix of backing files
                             1ULL << 25);         // The size of the maximum total allocation size.
                                                  // Metall reserves a contiguous region in virtual memory space with this size;
                                                  // however, it does not consume actual memory spaces in DRAM and file until
                                                  // the corresponding pages are touched.

    // Allocate and construct a vector object in the persistent memory with a name "vec"
    auto pvec = manager.construct<vector_t>        // Allocate and construct an object of vector_t
                        ("vec")                    // Name of the allocated object
                        (manager.get_allocator()); // Arguments passed to vector_t's constructor

    pvec->push_back(5); // Can use containers normally

    manager.sync(); // Explicitly sync with files

  } // Implicitly sync with files

  {
    // Reload the manager object
    metall::manager manager(metall::open_only, "/tmp/file_path");

    // Find the previously constructed object
    // Please do not forget to use ".first" at the end
    auto pvec = manager.find<vector_t>("vec").first;

    std::cout << (*pvec)[0] << std::endl; // Will print "5"

    manager.destroy<vector_t>("vec"); // Destroy the object
  }

  return 0;
}