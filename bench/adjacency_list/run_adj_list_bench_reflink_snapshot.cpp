// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <ftw.h>
#include <sys/stat.h>
#include <cstdlib>
#include <iostream>
#include <string>
#include <sstream>
#include <iomanip>
#include <metall/metall.hpp>
#include <metall/detail/time.hpp>
#include <metall/detail/file_clone.hpp>
#include "../data_structure/multithread_adjacency_list.hpp"
#include "bench_driver.hpp"

using namespace adjacency_list_bench;

using key_type = uint64_t;
using value_type = uint64_t;

using adjacency_list_type = data_structure::multithread_adjacency_list<
    key_type, value_type, typename metall::manager::allocator_type<std::byte>>;

ssize_t g_directory_total = 0;
int sum_file_sizes(const char *, const struct stat *statbuf, int typeflag) {
  if (typeflag == FTW_F) g_directory_total += statbuf->st_blocks * 512LL;
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

void run_df(const std::string &dir_path,
            const std::string &out_file_name = "/tmp/snapshot_storage_usage") {
  const std::string df_command("df ");
  const std::string full_command(df_command + " " + dir_path + " > " +
                                 out_file_name);

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

  std::cout << "Turn on the VERBOSE mode automatically" << std::endl;
  option.verbose = true;

  {
    metall::manager manager(metall::create_only,
                            option.datastore_path_list[0].c_str());

    // This function is called after inserting each chunk
    std::size_t snapshot_num = 0;
    auto snapshot_func = [&option, &manager, &snapshot_num]() {
      {
        const auto start = mdtl::elapsed_time_sec();
        manager.flush();
        const auto elapsed_time = mdtl::elapsed_time_sec(start);
        std::cout << "Flush took (s)\t" << elapsed_time << std::endl;
      }
      std::cout << "Original datastore size (GB)\t"
                << get_directory_size_gb(option.datastore_path_list[0])
                << std::endl;

      std::stringstream snapshot_id;
      snapshot_id << std::setw(4) << std::setfill('0')
                  << std::to_string(snapshot_num);

      {
        const auto snapshot_dir =
            option.datastore_path_list[0] + "-snapshot-" + snapshot_id.str();
        const auto start = mdtl::elapsed_time_sec();
        // Use copy() so that flush() is not called again
        metall::manager::copy(option.datastore_path_list[0].c_str(),
                              snapshot_dir.c_str());
        const auto elapsed_time = mdtl::elapsed_time_sec(start);
        std::cout << "Snapshot took (s)\t" << elapsed_time << std::endl;
        std::cout << "Snapshot datastore size (GB)\t"
                  << get_directory_size_gb(snapshot_dir) << std::endl;
        std::string out_file_name("storage-usage-snapshot-" +
                                  snapshot_id.str());
        run_df(snapshot_dir);
      }
      ++snapshot_num;
    };

    auto adj_list = manager.construct<adjacency_list_type>(
        option.adj_list_key_name.c_str())(manager.get_allocator<>());

    run_bench(option, adj_list, std::function<void()>{}, snapshot_func);
  }

  return 0;
}