// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_TAGS_HPP
#define METALL_TAGS_HPP

#include <metall/detail/char_ptr_holder.hpp>

namespace metall {

/// \brief Tag type to create the segment always.
/// The existing segment with the same name is over written.
struct create_only_t {};

/// \brief Tag to create the segment always.
/// The existing segment with the same name is over written.
[[maybe_unused]] static const create_only_t create_only{};

/// \brief Tag type to open an already created segment.
struct open_only_t {};

/// \brief Tag to open an already created segment.
[[maybe_unused]] static const open_only_t open_only{};

/// \brief Tag type to open an already created segment as read only.
struct open_read_only_t {};

/// \brief Tag to open an already created segment as read only.
[[maybe_unused]] static const open_read_only_t open_read_only{};

/// \brief Tag to construct anonymous instances.
[[maybe_unused]] static const mtlldetail::anonymous_instance_t
    *anonymous_instance = nullptr;

/// \brief Tag to construct a unique instance of a type
[[maybe_unused]] static const mtlldetail::unique_instance_t *unique_instance =
    nullptr;
}  // namespace metall

#endif  // METALL_TAGS_HPP
