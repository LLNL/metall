// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <random>
#include <boost/random/mersenne_twister.hpp>
#include <metall/utility/random.hpp>
#include <metall/detail/time.hpp>

template <typename rand_engine_type>
auto run_bench(const uint64_t num_generate) {
  rand_engine_type rand_engine(123);
  const auto s = metall::mtlldetail::elapsed_time_sec();
  for (uint64_t i = 0; i < num_generate; ++i) {
    [[maybe_unused]] volatile const uint64_t x = rand_engine();
  }
  const auto t = metall::mtlldetail::elapsed_time_sec(s);
  return t;
}

int main() {
  const uint64_t num_generate = (1ULL << 20ULL);
  std::cout << "Generate " << num_generate << " values" << std::endl;

  std::cout << "std::mt19937_64  \t" << run_bench<std::mt19937_64>(num_generate)
            << std::endl;
  std::cout << "boost::mt19937_64\t"
            << run_bench<boost::random::mt19937_64>(num_generate) << std::endl;
  std::cout << "xoshiro512++     \t"
            << run_bench<metall::utility::rand_512>(num_generate) << std::endl;
  std::cout << "xoshiro1024++    \t"
            << run_bench<metall::utility::rand_1024>(num_generate) << std::endl;

  return 0;
}
