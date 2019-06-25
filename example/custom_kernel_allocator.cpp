// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// -------------------------------------------------------------------------------- //
// This is an example of how to use a custom allocator for Metall's principal internal data structures.
// For instance, if you use a numa-aware custom allocator,
// you might be able to achieve better localities for Metall's management data
// -------------------------------------------------------------------------------- //

#include <string>
#include <boost/container/vector.hpp>

#include <metall/metall.hpp>
#include "numa_allocator.hpp"

using chunk_no_type = uint32_t;
static constexpr std::size_t k_chunk_size = 1UL << 21UL;
using kernel_allocator = numa_allocator<char>;
using manager_type = metall::basic_manager<chunk_no_type, k_chunk_size, kernel_allocator>;

using vector_t = boost::container::vector<int, typename manager_type::allocator_type<int>>;

int main() {
  const std::string manager_path("/tmp/file_path");

  {
    // Construct a manager object giving an object of the numa-aware allocator for Metall's kernel
    manager_type manager(metall::create_only, manager_path.c_str(), 1UL << 25UL, kernel_allocator());

    // Allocate and construct a vector object in the persistent memory with a name "vec"
    auto pvec = manager.construct<vector_t>("vec")(manager.get_allocator());

    pvec->push_back(5); // Can use containers normally
  }

  // ---------- Assume exit and restart the program at this point ---------- //

  {
    // Reload the manager object
    manager_type manager(metall::open_only, manager_path.c_str(), kernel_allocator());

    // Find the previously constructed object
    auto pvec = manager.find<vector_t>("vec").first;

    pvec->push_back(10); // Can restart to use containers normally

    std::cout << (*pvec)[0] << std::endl; // Will print "5"
    std::cout << (*pvec)[1] << std::endl; // Will print "10"

    manager.destroy<vector_t>("vec"); // Destroy the object
  }

  return 0;
}