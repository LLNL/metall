// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_KERNEL_MANAGER_KERNEL_FWD_HPP
#define METALL_KERNEL_MANAGER_KERNEL_FWD_HPP

namespace metall {
namespace kernel {

/// \brief Manager kernel class version 0
/// \tparam storage Storage manager.
/// \tparam segment_storage Segment storage manager.
/// \tparam chunk_no_type Type of chunk number
/// \tparam chunk_size Size of single chunk in byte
template <typename _storage, typename _segment_storage, typename _chunk_no_type,
          std::size_t _chunk_size>
class manager_kernel;

}  // namespace kernel
}  // namespace metall

#endif  // METALL_KERNEL_MANAGER_KERNEL_FWD_HPP
