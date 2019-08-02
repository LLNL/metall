// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
#define METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <vector>
#include <iterator>

#include <metall/v0/kernel/manager_kernel_fwd.hpp>

namespace metall {
namespace v0 {
namespace kernel {

template <typename chunk_no_type, std::size_t k_chunk_size, typename allocator_type>
template <typename out_stream_type>
void manager_kernel<chunk_no_type, k_chunk_size, allocator_type>::profile(out_stream_type *log_out) const {
  std::vector<std::size_t> num_used_chunks_per_bin(bin_no_mngr::num_bins(), 0);

  (*log_out) << std::fixed;
  (*log_out) << std::setprecision(2);

  (*log_out) << "\nChunk Information" << "\n";
  (*log_out) << "[chunk no]\t[obj size (0 is empty)]\t[occupancy rate (%)]" << "\n";
  for (chunk_no_type chunk_no = 0; chunk_no < m_chunk_directory.size(); ++chunk_no) {

    if (m_chunk_directory.empty_chunk(chunk_no)) {
      (*log_out) << chunk_no << "\t0\t0\n";
    } else {
      const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);
      ++num_used_chunks_per_bin[bin_no];

      const size_type object_size = bin_no_mngr::to_object_size(bin_no);

      if (bin_no < k_num_small_bins) {
        const std::size_t num_slots = m_chunk_directory.slots(chunk_no);
        const std::size_t num_occupied_slots = m_chunk_directory.occupied_slots(chunk_no);
        (*log_out) << chunk_no << "\t" << object_size
                   << "\t" << static_cast<double>(num_occupied_slots) / num_slots * 100 << "\n";
      } else {
        (*log_out) << chunk_no << "\t" << object_size << "\t100.0\n";
      }
    }
  }

  (*log_out) << "\nThe distribution of the sizes of being used chunks\n";
  (*log_out) << "(the number of used chunks at each object size)\n";
  (*log_out) << "[bin no]\t[obj size]\t[#of chunks (both full and non-full chunks)]\n";
  for (std::size_t bin_no = 0; bin_no < num_used_chunks_per_bin.size(); ++bin_no) {
    (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no)
               << "\t" << num_used_chunks_per_bin[bin_no] << "\n";
  }

  (*log_out) << "\nThe distribution of the sizes of non-full chunks\n";
  (*log_out) << "NOTE: only chunks used for small objects are in the bin directory\n";
  (*log_out) << "[bin no]\t[obj size]\t[#of non-full chunks]" << "\n";
  for (std::size_t bin_no = 0; bin_no < bin_no_mngr::num_small_bins(); ++bin_no) {
    std::size_t num_non_full_chunks = std::distance(m_bin_directory.begin(bin_no), m_bin_directory.end(bin_no));
    (*log_out) << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no) << "\t" << num_non_full_chunks << "\n";
  }
}

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
