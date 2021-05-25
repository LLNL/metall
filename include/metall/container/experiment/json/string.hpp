// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_STRING_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_STRING_HPP

#include <string>
#include <memory>

#include <boost/container/string.hpp>
namespace metall::container::experiment::json {

template <typename char_type,
          typename traits = std::char_traits<char_type>,
          typename allocator_type = std::allocator<char_type>>
using basic_string = boost::container::basic_string<char_type, traits, allocator_type>;

template <typename allocator_type = std::allocator<char>>
using string = basic_string<char, std::char_traits<char>, allocator_type>;
}

#endif //METALL_CONTAINER_EXPERIMENT_JSON_STRING_HPP
