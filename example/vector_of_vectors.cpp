// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

// This is an example of container of containers (or multi-level containers)
// with Metall

#include <iostream>
#include <boost/container/scoped_allocator.hpp>
#include <boost/container/vector.hpp>
#include <metall/metall.hpp>

namespace bc = boost::container;

// Vector of int
using inner_vector_allocator_type = metall::manager::allocator_type<int>;
using inner_vector_type = bc::vector<int, inner_vector_allocator_type>;

// Vector of vectors with scoped allocator adaptor
// In multi-level containers, you have to use scoped_allocator_adaptor in the
// most outer container so that the inner containers obtain their allocator
// arguments from the outer containers's scoped_allocator_adaptor See:
// https://en.cppreference.com/w/cpp/memory/scoped_allocator_adaptor
using outer_vector_allocator_type = bc::scoped_allocator_adaptor<
    metall::manager::allocator_type<inner_vector_type>>;
using outer_vector_type =
    bc::vector<inner_vector_type, outer_vector_allocator_type>;

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pvec = manager.construct<outer_vector_type>("vec-of-vecs")(
        manager.get_allocator<>());

    // In the following examples
    // the inner vectors obtain their allocator arguments from the outer
    // vector's scoped_allocator_adaptor

    // Pattern 1
    pvec->resize(1);
    (*pvec)[0].push_back(1);

    // Pattern 2
    pvec->emplace_back(1);
    (*pvec)[1][0] = 2;

    // Pattern 3
    std::vector<int> local_row = {3, 4};
    pvec->emplace_back(local_row.begin(), local_row.end());
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pvec = manager.find<outer_vector_type>("vec-of-vecs").first;

    // Check the result of pattern 1
    std::cout << (*pvec)[0][0] << std::endl;  // Will print out 1

    // Check the result of pattern 2
    std::cout << (*pvec)[1][0] << std::endl;  // Will print out 2

    // Check the results of pattern 3
    std::cout << (*pvec)[2][0] << std::endl;  // Will print out 3
    std::cout << (*pvec)[2][1] << std::endl;  // Will print out 4
  }

  return 0;
}