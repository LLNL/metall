// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_STRING_HPP
#define METALL_CONTAINER_STRING_HPP

#include <string>
#include <boost/container/string.hpp>

#include <metall/metall.hpp>

namespace metall::container {

template <class CharT,
          class Traits = std::char_traits<CharT>,
          class Allocator = metall::manager::allocator_type<CharT>>
using basic_string = boost::container::basic_string<CharT, Traits, Allocator>;

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;

}

#endif //METALL_CONTAINER_STRING_HPP
