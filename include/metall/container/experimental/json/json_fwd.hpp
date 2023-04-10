// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP

#include <string>
#include <variant>

#include <boost/json/src.hpp>

#include <metall/container/string.hpp>

/// \namespace metall::container::experimental
/// \brief Namespace for Metall containers in an experimental phase.
namespace metall::container::experimental {}

/// \namespace metall::container::experimental::json
/// \brief Namespace for Metall JSON container, which is in an experimental
/// phase.
namespace metall::container::experimental::json {}

namespace metall::container::experimental::json {

// Forward declaration
#if !defined(DOXYGEN_SKIP)
template <typename allocator_type> class value;

template <typename allocator_type> class array;

template <typename char_type, typename char_traits, typename _allocator_type>
class key_value_pair;

template <typename allocator_type> class indexed_object;

template <typename allocator_type> class compact_object;
#endif // DOXYGEN_SKIP

/// \brief JSON null.
using null_type = std::monostate;

/// \brief JSON object.
/// An object is a table key and value pairs.
/// The order of key-value pairs depends on the implementation.
template <typename allocator_type>
using object = compact_object<allocator_type>;

/// \brief JSON string.
template <typename char_t = char, typename traits = std::char_traits<char_t>,
          typename allocator_type = std::allocator<char_t>>
using string = metall::container::basic_string<char_t, traits, allocator_type>;

// Forward declaration
#if !defined(DOXYGEN_SKIP)

template <typename char_type, typename char_traits, typename allocator_type>
bool operator==(const key_value_pair<char_type, char_traits, allocator_type> &,
                const boost::json::key_value_pair &);

template <typename char_type, typename char_traits, typename allocator_type>
bool operator==(const boost::json::key_value_pair &,
                const key_value_pair<char_type, char_traits, allocator_type> &);

template <typename char_type, typename char_traits, typename allocator_type>
bool operator!=(const key_value_pair<char_type, char_traits, allocator_type> &,
                const boost::json::key_value_pair &);

template <typename char_type, typename char_traits, typename allocator_type>
bool operator!=(const boost::json::key_value_pair &,
                const key_value_pair<char_type, char_traits, allocator_type> &);

template <typename allocator_type>
bool operator==(const value<allocator_type> &, const boost::json::value &);

template <typename allocator_type>
bool operator==(const boost::json::value &, const value<allocator_type> &);

template <typename allocator_type>
bool operator!=(const value<allocator_type> &, const boost::json::value &);

template <typename allocator_type>
bool operator!=(const boost::json::value &, const value<allocator_type> &);

template <typename allocator_type>
bool operator==(const array<allocator_type> &, const boost::json::array &);

template <typename allocator_type>
bool operator==(const boost::json::array &, const array<allocator_type> &);

template <typename allocator_type>
bool operator!=(const array<allocator_type> &, const boost::json::array &);

template <typename allocator_type>
bool operator!=(const boost::json::array &, const array<allocator_type> &);

template <typename allocator_type>
bool operator==(const compact_object<allocator_type> &,
                const boost::json::object &);

template <typename allocator_type>
bool operator==(const boost::json::object &,
                const compact_object<allocator_type> &);

template <typename allocator_type>
bool operator!=(const compact_object<allocator_type> &,
                const boost::json::object &);

template <typename allocator_type>
bool operator!=(const boost::json::object &,
                const compact_object<allocator_type> &);

template <typename allocator_type>
bool operator==(const indexed_object<allocator_type> &,
                const boost::json::object &);

template <typename allocator_type>
bool operator==(const boost::json::object &,
                const indexed_object<allocator_type> &);

template <typename allocator_type>
bool operator!=(const indexed_object<allocator_type> &,
                const boost::json::object &);

template <typename allocator_type>
bool operator!=(const boost::json::object &,
                const indexed_object<allocator_type> &);

template <typename char_t, typename traits, typename allocator>
bool operator==(const string<char_t, traits, allocator> &,
                const boost::json::string &);

template <typename char_t, typename traits, typename allocator>
bool operator==(const boost::json::string &,
                const string<char_t, traits, allocator> &);

template <typename char_t, typename traits, typename allocator>
bool operator!=(const string<char_t, traits, allocator> &,
                const boost::json::string &);

template <typename char_t, typename traits, typename allocator>
bool operator!=(const boost::json::string &,
                const string<char_t, traits, allocator> &);

template <typename allocator_type, int indent_size>
void pretty_print(std::ostream &, const value<allocator_type> &);

template <typename allocator_type>
std::string serialize(const value<allocator_type> &);

template <typename allocator_type>
std::string serialize(const object<allocator_type> &);

template <typename allocator_type>
std::string serialize(const array<allocator_type> &);

template <typename char_type, typename traits, typename allocator_type>
std::string serialize(const string<char_type, traits, allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const value<allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const object<allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const array<allocator_type> &);

template <typename T, typename allocator_type>
value<allocator_type> value_from(T &&, const allocator_type &);

template <typename T, typename allocator_type>
T value_to(const value<allocator_type> &);

namespace jsndtl {
template <typename char_type, typename char_traits, typename allocator_type,
          typename other_key_value_pair_type>
bool general_key_value_pair_equal(
    const key_value_pair<char_type, char_traits, allocator_type> &,
    const other_key_value_pair_type &) noexcept;

template <typename allocator_type, typename other_object_type>
bool general_compact_object_equal(const compact_object<allocator_type> &,
                                  const other_object_type &) noexcept;

template <typename allocator_type, typename other_object_type>
bool general_indexed_object_equal(const indexed_object<allocator_type> &,
                                  const other_object_type &) noexcept;

template <typename allocator_type, typename other_array_type>
bool general_array_equal(const array<allocator_type> &,
                         const other_array_type &) noexcept;

template <typename char_t, typename traits, typename allocator,
          typename other_string_type>
bool general_string_equal(const string<char_t, traits, allocator> &,
                          const other_string_type &) noexcept;
} // namespace jsndtl
#endif // DOXYGEN_SKIP

} // namespace metall::container::experimental::json

#endif // METALL_CONTAINER_EXPERIMENT_JSON_JSON_FWD_HPP
