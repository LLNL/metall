// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TAGS_HPP
#define METALL_TAGS_HPP

#include <metall/detail/utility/char_ptr_holder.hpp>

namespace metall {
/// \brief Tag to create the segment always.
///
/// The existing segment with the same name is over written.
struct create_only_t {};
[[maybe_unused]] static const create_only_t create_only{};

/// \brief Tag to open an already created segment.
struct open_only_t {};
[[maybe_unused]] static const open_only_t open_only{};

/// \brief Tag to open a segment if it exists.
/// If it does not exist, creates a new one
struct open_or_create_t {};
[[maybe_unused]] static const open_or_create_t open_or_create{};

/// \brief Tag to open an already created segment as read only.
struct open_read_only_t {};
[[maybe_unused]] static const open_read_only_t open_read_only{};

/// \brief Tag to construct anonymous instances.
[[maybe_unused]] static const detail::utility::anonymous_instance_t *anonymous_instance = nullptr;

/// \brief Tag to construct a unique instance of a type
[[maybe_unused]] static const detail::utility::unique_instance_t *unique_instance = nullptr;
}

#endif //METALL_TAGS_HPP
