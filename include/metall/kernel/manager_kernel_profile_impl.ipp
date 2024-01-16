// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
#define METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP

#include <metall/kernel/manager_kernel_fwd.hpp>

namespace metall {
namespace kernel {

template <typename st, typename sst, typename cn, std::size_t cs>
template <typename out_stream_type>
void manager_kernel<st, sst, cn, cs>::profile(out_stream_type *log_out) {
  m_segment_memory_allocator.profile(log_out);
}

}  // namespace kernel
}  // namespace metall

#endif  // METALL_DETAIL_KERNEL_MANAGER_KERNEL_PROFILE_IMPL_IPP
