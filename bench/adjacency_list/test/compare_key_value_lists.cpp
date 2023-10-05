// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// Compare two key value files
/// Example
/// ./compare_key_value_lists kv_file1 kv_file2

#include <iostream>
#include <fstream>
#include <string>
#include <utility>
#include <unordered_map>
#include <metall/utility/hash.hpp>

using key_type = uint64_t;
using value_type = uint64_t;
using item_type = std::pair<key_type, value_type>;
using table_type =
    std::unordered_map<item_type, std::size_t, metall::utility::hash<>>;

void ingest_item(const std::string& file_name, table_type* table) {
  std::ifstream ifs(file_name);
  if (!ifs.is_open()) {
    std::cerr << "Cannot open " << file_name << std::endl;
    std::abort();
  }

  key_type key;
  value_type value;
  while (ifs >> key >> value) {
    const item_type item(key, value);
    if (table->count(item) == 0) {
      table->emplace(item, 0);
    }
    ++(table->at(item));
  }
}

int main(int argc, char* argv[]) {
  if (argc != 3) {
    std::cerr << "Wrong number of arguments" << std::endl;
    std::abort();
  }

  const std::string item_list_file_name1 = argv[1];
  const std::string item_list_file_name2 = argv[2];

  table_type table1;
  ingest_item(item_list_file_name1, &table1);

  table_type table2;
  ingest_item(item_list_file_name2, &table2);

  if (table1 != table2) {
    std::cerr << "Failed – the two lists are not the same" << std::endl;
    std::abort();
  }

  std::cout << "Succeeded!" << std::endl;

  return 0;
}