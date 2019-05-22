// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)


#ifndef METALL_V0_KERNEL_SEGMENT_HEADER_HPP
#define METALL_V0_KERNEL_SEGMENT_HEADER_HPP

template <std::size_t chunk_size>
struct segment_header {
  union {
    void* manager_kernel_address;
    char raw_buffer[chunk_size];
  };
};

#endif //METALL_V0_KERNEL_SEGMENT_HEADER_HPP
