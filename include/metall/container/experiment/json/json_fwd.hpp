// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP

namespace metall::container::experiment::json {

template <typename allocator_type>
class value;

template <typename allocator_type>
class object;

template <typename allocator_type>
class array;

template <typename char_type, typename char_traits, typename _allocator_type>
class basic_key_value_pair;

} // namespace metall::container::experiment::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
