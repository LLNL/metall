// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>

#include <boost/container/vector.hpp>

#include <metall/metall.hpp>  // Only one header file is required to be included

// Type alias
// This is the standard way to give a custom allocator to a container
using vector_t =
    boost::container::vector<int, metall::manager::allocator_type<int>>;

int main() {
  {
    // Construct a manager instance
    // A process can allocate multiple manager instances
    metall::manager manager(
        metall::create_only,  // Create a new one
        "/tmp/dir");          // The directory to store backing datastore

    // Allocate and construct a vector instance of vector_t.
    // The name "vec" is saved inside Metall datastore
    // and used to find the instance later.
    // The arguments in the last () are passed to the constructor of vector_t.
    auto pvec = manager.construct<vector_t>("vec")(manager.get_allocator());

    // From now on, the vector instance can be used normally.
    pvec->push_back(5);
  }  // Implicitly sync with backing files, i.e., sync() is called in
     // metall::manager's destructor

  // ---------- Assume exit and restart the program at this point ---------- //

  /// consistent() returns true if a Metall data store exits at the path and
  /// was closed property.
  if (metall::manager::consistent("/tmp/dir")) {
    // Reattach the manager instance
    metall::manager manager(metall::open_only, "/tmp/dir");

    // Find the previously constructed instance
    // Please do not forget to use ".first" at the end
    auto pvec = manager.find<vector_t>("vec").first;

    pvec->push_back(10);  // Can restart to use containers normally

    std::cout << (*pvec)[0] << std::endl;  // Will print "5"
    std::cout << (*pvec)[1] << std::endl;  // Will print "10"

    manager.destroy<vector_t>("vec");  // Destroy the instance
  } else {
    std::cerr << "Cannot open a Metall data store" << std::endl;
  }

  return 0;
}