// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_V0_KERNEL_MANAGER_KERNEL_FWD_HPP
#define METALL_V0_KERNEL_MANAGER_KERNEL_FWD_HPP

namespace metall {
namespace v0 {
namespace kernel {

/// \brief Manager kernel class version 0
/// \tparam chunk_no_type Type of chunk number
/// \tparam chunk_size Size of single chunk in byte
/// \tparam allocator_type Allocator used to allocate internal data
template <typename _chunk_no_type, std::size_t _chunk_size, typename _allocator_type>
class manager_kernel;

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_V0_KERNEL_MANAGER_KERNEL_FWD_HPP
