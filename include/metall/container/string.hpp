// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_STRING_HPP
#define METALL_CONTAINER_STRING_HPP

#include <string>
#include <boost/container/string.hpp>

#include <metall/metall.hpp>

namespace metall::container {

/// \brief A string container that uses Metall as its default allocator.
template <class CharT, class Traits = std::char_traits<CharT>,
          class Allocator = metall::manager::allocator_type<CharT>>
using basic_string = boost::container::basic_string<CharT, Traits, Allocator>;

/// \brief A string container that uses char as its character type and Metall as
/// its default allocator.
using string = basic_string<char>;

/// \brief A string container that uses wchar_t as its character type and Metall
/// as its default allocator.
using wstring = basic_string<wchar_t>;

}  // namespace metall::container

#endif  // METALL_CONTAINER_STRING_HPP
