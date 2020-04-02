// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \file

#ifndef METALL_METALL_HPP
#define METALL_METALL_HPP

#include <metall/basic_manager.hpp>

/// \namespace metall
/// \brief The top level of namespace of Metall
namespace metall {

/// \brief Default Metall manager class which is an alias of basic_manager with the default template parameters.
using manager = basic_manager<>;

} // namespace metall

#endif //METALL_METALL_HPP
