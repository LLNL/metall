// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_UUID_HPP
#define METALL_DETAIL_UTILITY_UUID_HPP

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace metall::mtlldetail {

using uuid = boost::uuids::uuid;
using uuid_random_generator = boost::uuids::random_generator;

}  // namespace metall::mtlldetail

#endif  // METALL_DETAIL_UTILITY_UUID_HPP