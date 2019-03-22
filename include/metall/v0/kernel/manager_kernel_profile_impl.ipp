//
// Created by Iwabuchi, Keita on 2019-03-21.
//

#ifndef METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
#define METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP

#include <iostream>
#include <fstream>
#include <string>
#include <iomanip>
#include <vector>
#include <iterator>

#include <metall/v0/kernel/manager_kernel.hpp>

namespace metall {
namespace v0 {
namespace kernel {

template <typename chunk_no_type, std::size_t k_chunk_size>
void manager_kernel<chunk_no_type, k_chunk_size>::profile(const std::string &log_file_name) const {
  std::ofstream log_file(log_file_name);
  if (!log_file.is_open()) {
    std::cerr << "Cannot open " << log_file_name << std::endl;
    return;
  }

  std::vector<std::size_t> bin_no_dist(bin_no_mngr::num_bins(), 0);

  log_file  << std::fixed;
  log_file << std::setprecision(2);

  log_file << "\nChunk Directory" << "\n";
  for (chunk_no_type chunk_no = 0; chunk_no <= m_chunk_directory.max_used_chunk_no(); ++chunk_no) {
    log_file << chunk_no << "\t";
    if (m_chunk_directory.empty_chunk(chunk_no)) {
      log_file << "[empty]" << std::endl;
    } else {
      const bin_no_type bin_no = m_chunk_directory.bin_no(chunk_no);
      ++bin_no_dist[bin_no];

      const size_type object_size = bin_no_mngr::to_object_size(bin_no);

      if (bin_no < k_num_small_bins) {
        const std::size_t num_slots = m_chunk_directory.slots(chunk_no);
        const std::size_t num_occupied_slots = m_chunk_directory.occupied_slots(chunk_no);
        log_file << "[" << object_size << "\t" << static_cast<double>(num_occupied_slots)/num_slots << "]\n";
      } else {
        log_file << "[" << object_size << "]\n";
      }

    }
  }

  log_file << "\nBeeing Used Bins" << "\n";
  for(std::size_t bin_no = 0; bin_no < bin_no_dist.size(); ++bin_no) {
    log_file << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no) << "\t" << bin_no_dist[bin_no] << "\n";
  }

  log_file << "\nBin Directory" << "\n";
  for (std::size_t bin_no = 0; bin_no < bin_no_mngr::num_small_bins(); ++bin_no) {
    std::size_t count = std::distance(m_bin_directory.begin(bin_no), m_bin_directory.end(bin_no));
    log_file << bin_no << "\t" << bin_no_mngr::to_object_size(bin_no) << "\t" << count << "\n";
  }

  log_file.close();
}

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
