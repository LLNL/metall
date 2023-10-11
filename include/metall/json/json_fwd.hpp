// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

/// \brief Contains forward declarations and type alias of Metall JSON
/// container.

#ifndef METALL_JSON_JSON_FWD_HPP
#define METALL_JSON_JSON_FWD_HPP

#include <string>
#include <variant>

#ifdef DOXYGEN_SKIP
/// \brief If defined, link with a buit Boost.JSON.
#define METALL_LINK_WITH_BOOST_JSON

/// \brief Include guard for boost/json/src.hpp
#define METALL_BOOST_JSON_SRC_INCLUDED
#endif

#ifdef METALL_LINK_WITH_BOOST_JSON
#include <boost/json.hpp>
#else
#ifndef METALL_BOOST_JSON_SRC_INCLUDED
#define METALL_BOOST_JSON_SRC_INCLUDED
#include <boost/json/src.hpp>
#endif  // METALL_BOOST_JSON_SRC_INCLUDED
#endif // METALL_LINK_WITH_BOOST_JSON

#include <metall/container/string.hpp>

/// \namespace metall::json
/// \brief Namespace for Metall JSON container, which is in an experimental
/// phase.
namespace metall::json {
/// \brief JSON null type.
using null_type = std::monostate;

/// \brief JSON basic string type.
template <typename char_t, typename traits, typename allocator_type>
using basic_string =
    metall::container::basic_string<char_t, traits, allocator_type>;

/// \brief JSON string.
template <typename allocator_type = std::allocator<std::byte>>
using string = basic_string<char, std::char_traits<char>, allocator_type>;
}  // namespace metall::json

// Forward declaration
#if !defined(DOXYGEN_SKIP)

namespace metall::json {

template <typename allocator_type = std::allocator<std::byte>>
class value;

template <typename allocator_type = std::allocator<std::byte>>
class array;

template <typename char_type = char,
          typename char_traits = std::char_traits<char_type>,
          typename allocator_type = std::allocator<char_type>>
class key_value_pair;

template <typename allocator_type = std::allocator<std::byte>>
class object;

template <typename allocator_type>
void swap(value<allocator_type> &, value<allocator_type> &) noexcept;

template <typename allocator_type>
void swap(string<allocator_type> &, string<allocator_type> &) noexcept;

template <typename allocator_type>
void swap(array<allocator_type> &, array<allocator_type> &) noexcept;

template <typename allocator_type>
void swap(object<allocator_type> &, object<allocator_type> &) noexcept;

template <typename char_type, typename char_traits, typename allocator_type>
void swap(key_value_pair<char_type, char_traits, allocator_type> &,
          key_value_pair<char_type, char_traits, allocator_type> &) noexcept;

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
bool operator==(const object<allocator_type> &, const boost::json::object &);

template <typename allocator_type>
bool operator==(const boost::json::object &, const object<allocator_type> &);

template <typename allocator_type>
bool operator!=(const object<allocator_type> &, const boost::json::object &);

template <typename allocator_type>
bool operator!=(const boost::json::object &, const object<allocator_type> &);

template <typename char_t, typename traits, typename allocator>
bool operator==(const basic_string<char_t, traits, allocator> &,
                const boost::json::string &);

template <typename char_t, typename traits, typename allocator>
bool operator==(const boost::json::string &,
                const basic_string<char_t, traits, allocator> &);

template <typename char_t, typename traits, typename allocator>
bool operator!=(const basic_string<char_t, traits, allocator> &,
                const boost::json::string &);

template <typename char_t, typename traits, typename allocator>
bool operator!=(const boost::json::string &,
                const basic_string<char_t, traits, allocator> &);

template <typename allocator_type, int indent_size = 2>
void pretty_print(std::ostream &, const value<allocator_type> &);

template <typename allocator_type>
std::string serialize(const value<allocator_type> &);

template <typename allocator_type>
std::string serialize(const object<allocator_type> &);

template <typename allocator_type>
std::string serialize(const array<allocator_type> &);

template <typename char_type, typename traits, typename allocator_type>
std::string serialize(const basic_string<char_type, traits, allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const value<allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const object<allocator_type> &);

template <typename allocator_type>
std::ostream &operator<<(std::ostream &, const array<allocator_type> &);

template <typename allocator_type = std::allocator<std::byte>>
value<allocator_type> parse(std::string_view,
                            const allocator_type &allocator = allocator_type());

template <typename T, typename allocator_type = std::allocator<std::byte>>
value<allocator_type> value_from(
    T &&, const allocator_type &allocator = allocator_type());

template <typename T, typename allocator_type>
T value_to(const value<allocator_type> &);

namespace jsndtl {

template <typename allocator_type, typename other_value_type>
bool general_value_equal(const value<allocator_type> &,
                         const other_value_type &) noexcept;

template <typename char_t, typename traits, typename allocator,
          typename other_string_type>
bool general_string_equal(const basic_string<char_t, traits, allocator> &,
                          const other_string_type &) noexcept;

template <typename allocator_type, typename other_array_type>
bool general_array_equal(const array<allocator_type> &,
                         const other_array_type &) noexcept;

template <typename allocator_type, typename other_object_type>
bool general_object_equal(const object<allocator_type> &,
                          const other_object_type &) noexcept;

template <typename char_type, typename char_traits, typename allocator_type,
          typename other_key_value_pair_type>
bool general_key_value_pair_equal(
    const key_value_pair<char_type, char_traits, allocator_type> &,
    const other_key_value_pair_type &) noexcept;
}  // namespace jsndtl

#endif  // DOXYGEN_SKIP

}  // namespace metall::json

#endif  // METALL_JSON_JSON_FWD_HPP
