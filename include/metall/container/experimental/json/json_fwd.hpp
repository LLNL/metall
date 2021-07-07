// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP

#include <variant>

/// \namespace metall::container::experimental
/// \brief Namespace for Metall containers in an experimental phase.
namespace metall::container::experimental {}

/// \namespace metall::container::experimental::json
/// \brief Namespace for Metall JSON container, which is in an experimental phase.
namespace metall::container::experimental::json {}

namespace metall::container::experimental::json {

#if !defined(DOXYGEN_SKIP)
// Forward declaration

template <typename allocator_type>
class value;

template <typename allocator_type>
class object;

template <typename allocator_type>
class array;

template <typename char_type, typename char_traits, typename _allocator_type>
class key_value_pair;

template <typename T, typename allocator_type>
inline value<allocator_type> value_from(T &&, allocator_type allocator = allocator_type());

template <typename T, typename allocator_type>
inline T value_to(const value<allocator_type>&);

#endif // DOXYGEN_SKIP

/// \brief JSON null.
using null_type = std::monostate;

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
