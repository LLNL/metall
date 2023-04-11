// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <boost/interprocess/managed_mapped_file.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include "kernel.hpp"

namespace bip = boost::interprocess;

int main(int argc, char *argv[]) {
  const auto option = simple_alloc_bench::parse_option(argc, argv);

  bip::file_mapping::remove(option.datastore_path.c_str());

  const std::size_t max_alloc_size =
      *(std::max_element(option.size_list.begin(), option.size_list.end())) *
      option.num_allocations;
  bip::managed_mapped_file mfile(
      bip::create_only, option.datastore_path.c_str(), max_alloc_size * 2);

  simple_alloc_bench::run_bench(simple_alloc_bench::option_type{},
                                mfile.get_allocator<std::byte>());

  bip::file_mapping::remove(option.datastore_path.c_str());

  return 0;
}