// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_OFFSET_PTR_HPP
#define METALL_OFFSET_PTR_HPP

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/utilities.hpp>

namespace metall {

/// \tparam T A type.
/// \brief Holds an offset between the address pointing at and itself.
template <typename T>
using offset_ptr = boost::interprocess::offset_ptr<T>;

/// \brief Convert a offset pointer to the corresponding raw pointer
using boost::interprocess::ipcdetail::to_raw_pointer;
}  // namespace metall

/// \example offset_pointer.cpp
/// This is an example of how to use offset_pointer.

#endif  // METALL_OFFSET_PTR_HPP
