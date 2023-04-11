// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_DETAIL_NAMED_PROXY_HPP
#define METALL_UTILITY_DETAIL_NAMED_PROXY_HPP

#include <boost/interprocess/detail/named_proxy.hpp>

/// \namespace metall::mtlldetail
/// \brief Namespace for implementation details
namespace metall::mtlldetail {

/// \brief Proxy class that implements named allocation syntax.
/// \tparam segment_manager segment manager to construct the object
/// \tparam T type of object to build
/// \tparam is_iterator passing parameters are normal object or iterators?
template <typename segment_manager, typename T, bool is_iterator>
using named_proxy = boost::interprocess::ipcdetail::named_proxy<segment_manager,
                                                                T, is_iterator>;

}  // namespace metall::mtlldetail

#endif  // METALL_UTILITY_DETAIL_NAMED_PROXY_HPP
