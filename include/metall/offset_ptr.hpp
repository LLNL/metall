// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_OFFSET_PTR_HPP
#define METALL_OFFSET_PTR_HPP

#include <boost/interprocess/offset_ptr.hpp>
#include <boost/interprocess/detail/utilities.hpp>

namespace metall {

/// \tparam T The type of the pointee.
/// \brief Holds an offset between the address pointing at and itself.
template <typename T>
using offset_ptr = boost::interprocess::offset_ptr<T>;

/// \brief Convert a offset pointer to the corresponding raw pointer
using boost::interprocess::ipcdetail::to_raw_pointer;
}

#endif //METALL_OFFSET_PTR_HPP
