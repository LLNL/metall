// Copyright 2021 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_EXPERIMENT_JSON_DETAIL_STRING_VIEW_HPP
#define METALL_CONTAINER_EXPERIMENT_JSON_DETAIL_STRING_VIEW_HPP

#include <string>
#include <memory>

namespace metall::container::experiment::json::jsndtl {

template <typename char_type,
          typename traits = std::char_traits<char_type>,
          typename pointer_type = char_type *>
class basic_string_view {
 private:
  using internal_pointer_type = typename std::pointer_traits<pointer_type>::template rebind<const char_type>;

 public:
  using value_type = char_type;
  using traits_type = traits;

  using pointer = char_type *;
  using const_pointer = const char_type *;
  using reference = char_type &;
  using const_reference = const char &;

  using const_iterator = const_pointer;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;

  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  basic_string_view() noexcept = default;

  explicit basic_string_view(const char *const s)
      : m_ptr_string(s),
        m_length(traits_type::length(s)) {}

  explicit basic_string_view(std::string_view s)
      : m_ptr_string(s.data()),
        m_length(s.length()) {}

  basic_string_view(const char *const s, const size_type len)
      : m_ptr_string(s),
        m_length(len) {}

  ~basic_string_view() noexcept {
    m_ptr_string = nullptr;
    m_length = 0;
  }

  constexpr int compare(const basic_string_view &v) const noexcept {
    std::cout << data() << " <> " << v.data() << " "
              << traits::compare(to_raw_pointer(data()), to_raw_pointer(v.data()), m_length) << std::endl;
    return traits::compare(to_raw_pointer(data()), to_raw_pointer(v.data()), m_length);
  }

  size_type size() const noexcept {
    return m_length;
  }

  size_type length() const noexcept {
    return m_length;
  }

  const_pointer data() const noexcept {
    return to_raw_pointer(m_ptr_string);
  }

  const_reference operator[](const size_type pos) const {
    return m_ptr_string[pos];
  }

 private:
  internal_pointer_type m_ptr_string{nullptr};
  std::size_t m_length{0};
};

template <typename char_type, typename traits, typename allocator_type>
inline std::size_t hash_value(const basic_string_view<char_type, traits, allocator_type> &value) {
  // std::cout << value.data() << " -> " << metall::mtlldetail::MurmurHash64A(value.data(), value.length(), 563466) << std::endl;
  return jsndtl::MurmurHash64A(value.data(), value.length(), 563466);
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator==(const basic_string_view<char_type, traits, allocator_type> &lhs,
                          const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return (lhs.compare(rhs) == 0);
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator!=(const basic_string_view<char_type, traits, allocator_type> &lhs,
                          const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return !(lhs == rhs);
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator<(const basic_string_view<char_type, traits, allocator_type> &lhs,
                         const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return lhs.compare(rhs) < 0;
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator<=(const basic_string_view<char_type, traits, allocator_type> &lhs,
                          const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return !(lhs > rhs);
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator>(const basic_string_view<char_type, traits, allocator_type> &lhs,
                         const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return rhs < lhs;
}

template <typename char_type, typename traits, typename allocator_type>
constexpr bool operator>=(const basic_string_view<char_type, traits, allocator_type> &lhs,
                          const basic_string_view<char_type, traits, allocator_type> &rhs) noexcept {
  return !(lhs < rhs);
}

template <typename char_type, typename traits, typename allocator_type>
std::ostream &operator<<(std::ostream &os, const basic_string_view<char_type, traits, allocator_type> &obj) {
  os << obj.data();
  return os;
}

template <typename allocator_type>
using string_view = basic_string_view<char, std::char_traits<char>, allocator_type>;

} // namespace json::jsndtl

#endif //METALL_CONTAINER_EXPERIMENT_JSON_DETAIL_STRING_VIEW_HPP
