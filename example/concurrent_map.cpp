// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#include <iostream>
#include <thread>

#include <metall/metall.hpp>
#include <metall/container/concurrent_map.hpp>

using map_type = metall::container::concurrent_map<
    char, int, std::less<char>, std::hash<char>,
    metall::manager::allocator_type<std::pair<const char, int>>>;

bool insert_func1(const char key, const int value, map_type *pmap) {
  return pmap->insert(std::make_pair(key, value));
}

bool insert_func2(const char key, const int value, map_type *pmap) {
  auto val = std::make_pair(key, value);
  return pmap->insert(std::move(val));
}

bool insert_func3(const char key, const int value, map_type *pmap) {
  const auto const_val = std::make_pair(key, value);
  return pmap->insert(const_val);
}

// Scoped mutex style value update
void scoped_edit(const char key, const int value, map_type *pmap) {
  auto ret = pmap->scoped_edit(key);
  ret.first = value;
}

// Function style value update
void edit(const char key, const int value, map_type *pmap) {
  pmap->edit(key, [&value](int &mapped_value) {
    mapped_value = value;  // Key 'b' has value '20' now.
  });
}

int main() {
  {
    metall::manager manager(metall::create_only, "/tmp/datastore");
    auto pmap = manager.construct<map_type>("map")(manager.get_allocator<>());

    // Insert elements concurrently, using the 3 styles
    // Of course, one can use just one style concurrently
    {
      std::thread t1(insert_func1, 'a', 0, pmap);
      std::thread t2(insert_func2, 'b', 1, pmap);
      std::thread t3(insert_func3, 'c', 2, pmap);

      // Wait for the threads to finish
      t1.join();
      t2.join();
      t3.join();
    }

    // Edit elements concurrently using the 2 styles
    {
      std::thread t1(scoped_edit, 'a', 10, pmap);
      std::thread t2(edit, 'b', 20, pmap);

      // Wait for the threads to finish
      t1.join();
      t2.join();
    }
  }

  {
    metall::manager manager(metall::open_only, "/tmp/datastore");
    auto pmap = manager.find<map_type>("map").first;

    // Of course, one can update values after reattaching
    edit('c', 30, pmap);

    for (auto itr = pmap->cbegin(), end = pmap->cend(); itr != end; ++itr) {
      // Will show the following lines (with a undetermined order):
      // a 10
      // b 20
      // c 30
      std::cout << itr->first << " " << itr->second << std::endl;
    }
  }

  return 0;
}