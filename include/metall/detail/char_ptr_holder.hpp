// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_CHAR_OTR_HOLDER_HPP
#define METALL_DETAIL_UTILITY_CHAR_OTR_HOLDER_HPP

#include <boost/interprocess/detail/segment_manager_helper.hpp>

namespace metall::mtlldetail {

template <typename char_type>
using char_ptr_holder =
    boost::interprocess::ipcdetail::char_ptr_holder<char_type>;

enum instance_kind { anonymous_kind, named_kind, unique_kind };

using anonymous_instance_t =
    boost::interprocess::ipcdetail::anonymous_instance_t;
using unique_instance_t = boost::interprocess::ipcdetail::unique_instance_t;

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_CHAR_OTR_HOLDER_HPP
