// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_OBJECT_HPP
#define METALL_OBJECT_HPP

#include <metall/json/json_fwd.hpp>
#include <metall/json/details/compact_object.hpp>

namespace metall::json {

/// \brief JSON object.
/// An object is a table key and value pairs.
/// The order of key-value pairs depends on the implementation.
#ifdef DOXYGEN_SKIP
template <typename allocator_type = std::allocator<std::byte>>
#else
template <typename allocator_type>
#endif
class object : public jsndtl::compact_object<allocator_type> {
  using jsndtl::compact_object<allocator_type>::compact_object;
};

/// \brief Swap value instances.
template <typename allocator_type>
inline void swap(object<allocator_type> &lhd,
                 object<allocator_type> &rhd) noexcept {
  lhd.swap(rhd);
}

namespace jsndtl {

/// \brief Provides 'equal' calculation for other object types that have the
/// same interface as the object class.
template <typename allocator_type, typename other_object_type>
inline bool general_object_equal(
    const object<allocator_type> &object,
    const other_object_type &other_object) noexcept {
  return general_compact_object_equal(object, other_object);
}

}  // namespace jsndtl

}  // namespace metall::json

#endif  // METALL_OBJECT_HPP
