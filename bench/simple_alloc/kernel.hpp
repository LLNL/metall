// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_BENCH_BASIC_KERNEL_HPP
#define METALL_BENCH_BASIC_KERNEL_HPP

#include <unistd.h>
#include <iostream>
#include <memory>
#include <vector>
#include <type_traits>
#include <chrono>
#include <random>
#include <thread>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <metall/detail/time.hpp>
#include <metall/detail/utilities.hpp>

namespace simple_alloc_bench {

namespace {
namespace mdtl = metall::mtlldetail;
}

struct option_type {
  std::size_t num_allocations = 1 << 20;
  std::vector<std::size_t> size_list{8, 4096};
  std::string datastore_path{"/tmp/datastore"};
  bool run_parallel_bench = false;
};

option_type parse_option(int argc, char **argv) {
  int p;
  option_type option;
  while ((p = ::getopt(argc, argv, "o:n:p")) != -1) {
    switch (p) {
      case 'o':
        option.datastore_path = optarg;
        break;

      case 'n':
        option.num_allocations = std::stold(optarg);
        break;

      case 'p':
        option.run_parallel_bench = true;
        break;

      default:
        std::cerr << "Invalid option" << std::endl;
        std::abort();
    }
  }

  if (optind < argc) {
    option.size_list.clear();
    for (int index = optind; index < argc; index++) {
      option.size_list.emplace_back(std::stoll(argv[index]));
    }
  }

  return option;
}

template <typename byte_allocator_type>
void allocate_sequential(
    byte_allocator_type byte_allocator,
    const std::vector<std::size_t> &size_list,
    std::vector<typename byte_allocator_type::pointer> *allocated_addr_list) {
  static_assert(
      std::is_same<
          typename std::allocator_traits<byte_allocator_type>::value_type,
          std::byte>::value,
      "The value_type of byte_allocator_type must be std::byte");
  for (std::size_t i = 0; i < size_list.size(); ++i) {
    (*allocated_addr_list)[i] = byte_allocator.allocate(size_list[i]);
    if (!(*allocated_addr_list)[i]) {
      std::cerr << "Failed allocation" << std::endl;
      std::abort();
    }
  }
}

template <typename byte_allocator_type>
void deallocate_sequential(
    byte_allocator_type byte_allocator,
    const std::vector<std::size_t> &size_list,
    const std::vector<typename byte_allocator_type::pointer>
        &allocated_addr_list) {
  static_assert(
      std::is_same<
          typename std::allocator_traits<byte_allocator_type>::value_type,
          std::byte>::value,
      "The value_type of byte_allocator_type must be std::byte");
  for (std::size_t i = 0; i < size_list.size(); ++i) {
    byte_allocator.deallocate(allocated_addr_list[i], size_list[i]);
  }
}

template <typename byte_allocator_type>
void allocate_parallel(
    byte_allocator_type byte_allocator,
    const std::vector<std::size_t> &size_list,
    std::vector<typename byte_allocator_type::pointer> *allocated_addr_list) {
  static_assert(
      std::is_same<
          typename std::allocator_traits<byte_allocator_type>::value_type,
          std::byte>::value,
      "The value_type of byte_allocator_type must be std::byte");

  std::vector<std::thread *> threads(std::thread::hardware_concurrency(),
                                     nullptr);
  for (std::size_t t = 0; t < threads.size(); ++t) {
    const auto range =
        metall::mtlldetail::partial_range(size_list.size(), t, threads.size());

    threads[t] = new std::thread(
        [range](byte_allocator_type byte_allocator,
                const std::vector<std::size_t> &size_list,
                std::vector<typename byte_allocator_type::pointer>
                    *allocated_addr_list) {
          for (std::size_t i = range.first; i < range.second; ++i) {
            (*allocated_addr_list)[i] = byte_allocator.allocate(size_list[i]);
            if (!(*allocated_addr_list)[i]) {
              std::cerr << "Failed allocation" << std::endl;
              std::abort();
            }
          }
        },
        byte_allocator, size_list, allocated_addr_list);
  }

  for (auto thread : threads) {
    thread->join();
  }
}

template <typename byte_allocator_type>
void deallocate_parallel(
    byte_allocator_type byte_allocator,
    const std::vector<std::size_t> &size_list,
    const std::vector<typename byte_allocator_type::pointer>
        &allocated_addr_list) {
  static_assert(
      std::is_same<
          typename std::allocator_traits<byte_allocator_type>::value_type,
          std::byte>::value,
      "The value_type of byte_allocator_type must be std::byte");

  std::vector<std::thread *> threads(std::thread::hardware_concurrency(),
                                     nullptr);
  for (std::size_t t = 0; t < threads.size(); ++t) {
    const auto range =
        metall::mtlldetail::partial_range(size_list.size(), t, threads.size());

    threads[t] = new std::thread(
        [range](byte_allocator_type byte_allocator,
                const std::vector<std::size_t> &size_list,
                const std::vector<typename byte_allocator_type::pointer>
                    &allocated_addr_list) {
          for (std::size_t i = range.first; i < range.second; ++i) {
            byte_allocator.deallocate(allocated_addr_list[i], size_list[i]);
          }
        },
        byte_allocator, size_list, allocated_addr_list);
  }

  for (auto thread : threads) {
    thread->join();
  }
}

template <typename alloc_function, typename dealloc_function>
void measure_time(const int num_runs, alloc_function alloc_func,
                  dealloc_function dealloc_func) {
  std::vector<double> alloc_times(num_runs, 0.0);
  std::vector<double> dealloc_times(num_runs, 0.0);
  for (int i = 0; i < num_runs; ++i) {
    {
      const auto start = mdtl::elapsed_time_sec();
      alloc_func();
      const auto elapsed_time = mdtl::elapsed_time_sec(start);
      alloc_times[i] = elapsed_time;
    }
    {
      const auto start = mdtl::elapsed_time_sec();
      dealloc_func();
      const auto elapsed_time = mdtl::elapsed_time_sec(start);
      dealloc_times[i] = elapsed_time;
    }
  }

  auto print_results = [](std::vector<double> &times) {
    std::sort(times.begin(), times.end());
    std::cout << "Min\t" << times[0] << std::endl;
    const double median =
        (times.size() % 2)
            ? (times[times.size() / 2 - 1] + times[times.size() / 2])
            : times[times.size() / 2];
    std::cout << "Median\t" << median << std::endl;
    std::cout << "Max\t" << times[times.size() - 1] << std::endl;
  };

  std::cout << "Allocation time (s)" << std::endl;
  print_results(alloc_times);
  std::cout << "\nDeallocation time (s)" << std::endl;
  print_results(dealloc_times);
}

template <typename allocator_type>
void run_bench(const option_type &option, const allocator_type allocator) {
  using byte_allocator_type = typename std::allocator_traits<
      allocator_type>::template rebind_alloc<std::byte>;
  byte_allocator_type byte_allocator(allocator);

  std::vector<std::size_t> allocation_request_list(option.num_allocations);
  std::vector<typename byte_allocator_type::pointer> allocated_addr_list(
      option.num_allocations);

  std::cout << std::fixed;
  std::cout << std::setprecision(2);

  for (std::size_t i = 0; i < option.size_list.size() + 1; ++i) {
    if (i < option.size_list.size()) {
      std::cout << "\n----- Allocation/deallocation with "
                << option.size_list[i] << " byte -----" << std::endl;
      std::fill(allocation_request_list.begin(), allocation_request_list.end(),
                option.size_list[i]);
      std::fill(allocated_addr_list.begin(), allocated_addr_list.end(),
                nullptr);
    } else {
      std::cout << "\n----- Allocation/deallocation with mixed sizes -----"
                << std::endl;
      std::mt19937_64 rand(
          std::chrono::system_clock::now().time_since_epoch().count());
      std::uniform_int_distribution<> dist(0, option.size_list.size() - 1);
      for (auto &request : allocation_request_list) {
        request = option.size_list[dist(rand)];
      }
    }

    std::cout << "[Sequential]" << std::endl;
    measure_time(
        10,
        [byte_allocator, &allocation_request_list, &allocated_addr_list]() {
          allocate_sequential(byte_allocator, allocation_request_list,
                              &allocated_addr_list);
        },
        [byte_allocator, &allocation_request_list, &allocated_addr_list]() {
          deallocate_sequential(byte_allocator, allocation_request_list,
                                allocated_addr_list);
        });

    if (!option.run_parallel_bench) continue;

    std::cout << "\n[Parallel with " << std::thread::hardware_concurrency()
              << " threads ]" << std::endl;
    measure_time(
        10,
        [byte_allocator, &allocation_request_list, &allocated_addr_list]() {
          allocate_parallel(byte_allocator, allocation_request_list,
                            &allocated_addr_list);
        },
        [byte_allocator, &allocation_request_list, &allocated_addr_list]() {
          deallocate_parallel(byte_allocator, allocation_request_list,
                              allocated_addr_list);
        });
  }
}

}  // namespace simple_alloc_bench
#endif  // METALL_BENCH_BASIC_KERNEL_HPP