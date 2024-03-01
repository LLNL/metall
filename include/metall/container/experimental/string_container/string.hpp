// Copyright 2024 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_STRING_HPP
#define METALL_CONTAINER_EXPERIMENT_STRING_CONTAINER_STRING_HPP

#include <memory>

#include <metall/container/string.hpp>

#include <metall/container/experimental/string_container/string_table.hpp>

namespace metall::container::experimental::string_container {

/// \brief A string container that uses the string_table internally instead of
/// storing keys independently.
template <typename StringTable>
class string {
 private:
  using string_table_type = StringTable;
  using locator_type = typename string_table_type::locator_type;

 public:
  explicit string(string_table_type *const string_table)
      : m_string_table(string_table) {
    m_locator = m_string_table->insert("");
  }

  string(string_table_type *const string_table, const locator_type locator)
      : m_string_table(string_table), m_locator(locator) {}

  string(const string &other) = default;
  string(string &&other) noexcept = default;
  string &operator=(const string &other) = default;
  string &operator=(string &&other) noexcept = default;

  ~string() = default;

  string &operator=(const char *s) {
    m_locator = m_string_table->insert(s);
    return *this;
  }

  string &operator=(const char ch) {
    m_locator = m_string_table->insert(ch);
    return *this;
  }

  template <class CharT, class Traits, class Alloc>
  string &operator=(const std::basic_string<CharT, Traits, Alloc> &s) {
    m_locator = m_string_table->insert(s.c_str());
    return *this;
  }

  template <class CharT, class Traits>
  string &operator=(const std::basic_string_view<CharT, Traits> s) {
    m_locator = m_string_table->insert(s.data());
    return *this;
  }

  std::size_t size() const { return m_string_table->to_key(m_locator).size(); }

  std::size_t length() const { return this->size(); }

  const char *c_str() const { return m_string_table->to_key(m_locator).data(); }

  const char *data() const { return this->c_str(); }

  bool empty() const { return m_string_table->to_key(m_locator).empty(); }

 private:
  using allocator_type = typename string_table_type::allocator_type;
  using string_table_ptr_type =
      typename std::pointer_traits<typename std::allocator_traits<
          allocator_type>::pointer>::template rebind<string_table_type>;

  string_table_ptr_type m_string_table{nullptr};
  locator_type m_locator{string_table_type::invalid_locator};
};

// compare operators
template <typename StringTable>
bool operator==(const string<StringTable> &lhs,
                const string<StringTable> &rhs) {
  return std::strcmp(lhs.c_str(), rhs.c_str()) == 0;
}

template <typename StringTable>
bool operator!=(const string<StringTable> &lhs,
                const string<StringTable> &rhs) {
  return !(lhs == rhs);
}

template <typename StringTable>
bool operator<(const string<StringTable> &lhs, const string<StringTable> &rhs) {
  return std::strcmp(lhs.c_str(), rhs.c_str()) < 0;
}

template <typename StringTable>
bool operator==(const string<StringTable> &lhs, const char *rhs) {
  return std::strcmp(lhs.c_str(), rhs) == 0;
}

template <typename StringTable>
bool operator!=(const string<StringTable> &lhs, const char *rhs) {
  return !(lhs == rhs);
}

template <typename StringTable>
bool operator<(const string<StringTable> &lhs, const char *rhs) {
  return std::strcmp(lhs.c_str(), rhs) < 0;
}

template <typename StringTable>
bool operator==(const char *lhs, const string<StringTable> &rhs) {
  return rhs == lhs;
}

template <typename StringTable>
bool operator!=(const char *lhs, const string<StringTable> &rhs) {
  return rhs != lhs;
}

template <typename StringTable>
bool operator<(const char *lhs, const string<StringTable> &rhs) {
  return rhs > lhs;
}

// stream operators
template <typename StringTable>
std::ostream &operator<<(std::ostream &os, const string<StringTable> &str) {
  os << str.c_str();
  return os;
}

}  // namespace metall::container::experimental::string_container
#endif