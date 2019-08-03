// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#ifndef METALL_DETAIL_V0_KERNEL_SEGMENT_STORAGE_HPP
#define METALL_DETAIL_V0_KERNEL_SEGMENT_STORAGE_HPP

#ifdef METALL_USE_ANONYMOUS_MAP
#include <metall/v0/kernel/anonymous_mapped_segment_storage.hpp>
#else
#include <metall/v0/kernel/file_mapped_segment_storage.hpp>
#endif

namespace metall {
namespace v0 {
namespace kernel {

template <typename offset_type, typename size_type, std::size_t header_size>
using segment_storage =

#ifdef METALL_USE_ANONYMOUS_MAP
    anonymous_mapped_segment_storage<offset_type, size_type, header_size>;
#else
    file_mapped_segment_storage<offset_type, size_type, header_size>;
#endif

} // namespace kernel
} // namespace v0
} // namespace metall
#endif //METALL_DETAIL_V0_KERNEL_SEGMENT_STORAGE_HPP
