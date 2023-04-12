// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_BIN_NUMBER_MANAGER_HPP
#define METALL_DETAIL_KERNEL_BIN_NUMBER_MANAGER_HPP

#include <cstddef>
#include <type_traits>

#include <metall/kernel/object_size_manager.hpp>
#include <metall/detail/utilities.hpp>

namespace metall {
namespace kernel {

/// \brief Bin number manager
template <std::size_t k_chunk_size, std::size_t k_max_object_size>
class bin_number_manager {
 public:
  using size_type = std::size_t;

 private:
  using object_size_mngr = object_size_manager<k_chunk_size, k_max_object_size>;
  static constexpr size_type k_num_small_bins =
      object_size_mngr::num_small_sizes();
  static constexpr size_type k_num_large_bins =
      object_size_mngr::num_large_sizes();
  static constexpr size_type k_num_bins = k_num_small_bins + k_num_large_bins;

 public:
  using bin_no_type = typename mdtl::unsigned_variable_type<k_num_bins>::type;

  bin_number_manager() = delete;
  ~bin_number_manager() = delete;
  bin_number_manager(const bin_number_manager &) = delete;
  bin_number_manager(bin_number_manager &&) noexcept = delete;
  bin_number_manager &operator=(const bin_number_manager &) = delete;
  bin_number_manager &operator=(bin_number_manager &&) noexcept = delete;

  static constexpr size_type num_bins() noexcept { return k_num_bins; }

  static constexpr size_type num_small_bins() noexcept {
    return k_num_small_bins;
  }

  static constexpr size_type num_large_bins() noexcept {
    return k_num_large_bins;
  }

  static constexpr bin_no_type to_bin_no(const size_type size) noexcept {
    return object_size_mngr::index(size);
  }

  static constexpr size_type to_object_size(const bin_no_type bin_no) noexcept {
    return object_size_mngr::at(bin_no);
  }
};

}  // namespace kernel
}  // namespace metall
#endif  // METALL_DETAIL_KERNEL_BIN_NUMBER_MANAGER_HPP
