// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>

#include <boost/container/vector.hpp>

#include <metall/metall.hpp>
#include <metall_utility/fallback_allocator_adaptor.hpp>

// The line below is the only change required to use fallback_allocator_adaptor.
// Wraps up 'metall::manager::allocator_type<..>' with fallback_allocator_adaptor.
using allocator_t = metall::utility::fallback_allocator_adaptor<metall::manager::allocator_type<int>>;

using vector_t = boost::container::vector<int, allocator_t>;

int main() {

  // Allocation with Metall
  // The code below works with both 'fallback_allocator_adaptor<..>' and 'metall::manager::allocator_type<...>'.
  {
    metall::manager manager(metall::create_only, "/tmp/dir");
    auto pvec = manager.construct<vector_t>("vec")(manager.get_allocator());
    pvec->push_back(1);
    std::cout << (*pvec)[0] << std::endl;
  }

  // Allocation w/o Metall, i.e., uses a heap allocator such as malloc().
  // This code causes a build error if fallback_allocator_adaptor is not used.
  {
    vector_t vec;
    vec.push_back(2);
    std::cout << vec[0] << std::endl;
  }

  return 0;
}