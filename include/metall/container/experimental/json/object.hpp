// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_OBJECT_HPP
#define METALL_CONTAINER_EXPERIMENT_OBJECT_HPP

#include <metall/container/experimental/json/json_fwd.hpp>
#include <metall/container/experimental/json/details/compact_object.hpp>

namespace metall::container::experimental::json {

/// \brief JSON object.
/// An object is a table key and value pairs.
/// The order of key-value pairs depends on the implementation.
template <typename allocator_type>
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

/// \brief Provides 'equal' calculation for other object types that have the same interface as the object class.
template <typename allocator_type, typename other_object_type>
inline bool general_object_equal(const object<allocator_type> &object,
                                 const other_object_type &other_object) noexcept {
  return object == other_object;
}

} // namespace jsndtl

} // namespace metall::container::experimental::json

#endif //METALL_CONTAINER_EXPERIMENT_OBJECT_HPP
