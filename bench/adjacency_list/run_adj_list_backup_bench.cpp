// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <ftw.h>
#include <sys/stat.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <memory>

#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using adj_list_t = data_structure::multithread_adjacency_list<key_type,
                                                              value_type,
                                                              typename metall::manager::allocator_type<std::byte>>;

ssize_t g_directory_total = 0;
int sum_file_sizes(const char *, const struct stat *statbuf, int typeflag) {
  if (typeflag == FTW_F)
    g_directory_total += statbuf->st_blocks * 512LL;
  return 0;
}

double get_directory_size_gb(const std::string &dir_path) {
  if (::ftw(dir_path.c_str(), &sum_file_sizes, 1) == -1) {
    g_directory_total = 0;
    return -1;
  }
  const auto wk = g_directory_total;
  g_directory_total = 0;
  return (double)wk / (1ULL << 30ULL);
}

void run_df(const std::string &dir_path, const std::string &out_file_name = "/tmp/snapshot_storage_usage") {
  const std::string df_command("df ");
  const std::string full_command(df_command + " " + dir_path + " > " + out_file_name);

  std::system(full_command.c_str());
  std::ifstream ifs(out_file_name);
  std::string buf;
  std::getline(ifs, buf);
  std::cout << "[df] " << buf << std::endl;
  std::getline(ifs, buf);
  std::cout << "[df] " << buf << std::endl;
}

int main(int argc, char *argv[]) {

  bench_options option;
  if (!parse_options(argc, argv, &option)) {
    std::abort();
  }

  if (option.datastore_path_list.empty()) {
    std::cerr << "Datastore path is required" << std::endl;
    std::abort();
  }

  std::unique_ptr<metall::manager> manager;

  const auto data_store_path = option.datastore_path_list[0];
  const auto backup_path = data_store_path + "-backup";

  // Create backup (take snapshot)
  if (option.append) {
    run_df(data_store_path.c_str());
    const auto start = mdtl::elapsed_time_sec();
    metall::manager::copy(data_store_path.c_str(), backup_path.c_str());
    const auto elapsed_time = mdtl::elapsed_time_sec(start);
    std::cout << "Taking backup took (s)\t" << elapsed_time << std::endl;
    std::cout << "Backup datastore size (GB)\t" << get_directory_size_gb(backup_path) << std::endl;
    run_df(data_store_path.c_str());
  }

  // Ingest edges
  {
    manager = (option.append) ? std::make_unique<metall::manager>(metall::open_only,data_store_path.c_str())
                              : std::make_unique<metall::manager>(metall::create_only,data_store_path.c_str());

    auto adj_list = (option.append) ? manager->find<adj_list_t>(option.adj_list_key_name.c_str()).first
                                    : manager->construct<adj_list_t>(option.adj_list_key_name.c_str())(manager->get_allocator<>());
    run_bench(option, adj_list);
  }

  // Close Metall
  {
    const auto start = metall::mtlldetail::elapsed_time_sec();
    manager.reset(nullptr);
    const auto elapsed_time = metall::mtlldetail::elapsed_time_sec(start);
    std::cout << "Closing Metall took (s)\t" << elapsed_time << std::endl;
  }

  {
    std::cout << "Remove backup" << std::endl;
    metall::manager::remove(backup_path.c_str());
  }

  return 0;
}