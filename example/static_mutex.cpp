// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <thread>

#include <metall/utility/mutex.hpp>

static constexpr std::size_t k_num_mutexes = 2;

void mutex_work(const int key, const int value, int* array) {
  {  // A mutex block
    const int index = key % k_num_mutexes;
    auto guard = metall::utility::mutex::mutex_lock<k_num_mutexes>(index);
    array[index] = array[index] + value;  // do some mutex work
  }                                       // The mutex is released here
}

int main() {
  int array[k_num_mutexes] = {0, 0};

  // Launch multiple concurrent jobs
  std::thread t1(mutex_work, 0, 1, array);  // Add 1 to array[0]
  std::thread t2(mutex_work, 1, 2, array);  // Add 2 to array[1]
  std::thread t3(mutex_work, 2, 3, array);  // Add 3 to array[0]
  std::thread t4(mutex_work, 3, 4, array);  // Add 4 to array[1]

  // Wait for the threads to finish
  t1.join();
  t2.join();
  t3.join();
  t4.join();

  std::cout << array[0] << std::endl;  // Will show 4
  std::cout << array[1] << std::endl;  // Will show 6

  return 0;
}