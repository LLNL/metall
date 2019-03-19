// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_IN_PLACE_INTERFACE_HPP
#define METALL_DETAIL_UTILITY_IN_PLACE_INTERFACE_HPP

#include <boost/interprocess/detail/in_place_interface.hpp>

namespace metall {
namespace detail {
namespace utility {

using in_place_interface = boost::interprocess::ipcdetail::in_place_interface;

} // namespace utility
} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_UTILITY_IN_PLACE_INTERFACE_HPP
