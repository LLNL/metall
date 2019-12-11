// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_COTAINER_CONCURRENT_MAP_HPP
#define METALL_COTAINER_CONCURRENT_MAP_HPP

#include <functional>
#include <memory>
#include <boost/container/vector.hpp>
#include <boost/container/map.hpp>
#include <boost/container/scoped_allocator.hpp>
#include <metall_utility/mutex.hpp>
#include <metall_utility/container_of_containers_iterator_adaptor.hpp>

namespace metall::container {

template <typename _key_type,
          typename _mapped_type,
          typename _compare = std::less<_key_type>,
          typename _bank_no_hasher = std::hash<_key_type>,
          typename _allocator = std::allocator<std::pair<const _key_type, _mapped_type>>,
    int k_num_banks = 1024>
class concurrent_map {
 private:
  template <typename T>
  using other_allocator_type = typename std::allocator_traits<_allocator>::template rebind_alloc<T>;

  using internal_map_type = boost::container::map<_key_type, _mapped_type, _compare, _allocator>;
  using banked_map_allocator_type = other_allocator_type<internal_map_type>;
  using banked_map_type = std::vector<internal_map_type,
                                      boost::container::scoped_allocator_adaptor<banked_map_allocator_type>>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using key_type = typename internal_map_type::key_type;
  using mapped_type = typename internal_map_type::mapped_type;
  using value_type = typename internal_map_type::value_type;
  using size_type = typename internal_map_type::size_type;

  using const_iterator = metall_utility::container_of_containers_iterator_adaptor<typename banked_map_type::const_iterator,
                                                                                  typename internal_map_type::const_iterator>;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  explicit concurrent_map(const _allocator &allocator = _allocator())
      : m_banked_map(k_num_banks, allocator) {}

  // -------------------------------------------------------------------------------- //
  // Public methods
  // -------------------------------------------------------------------------------- //
  // ---------------------------------------- Modifier ---------------------------------------- //
  size_type count(const key_type &key) {
    const auto bank_no = calc_bank_no(key);
    return m_banked_map[bank_no].count(key);
  }

  bool insert(value_type &&value) {
    const auto bank_no = calc_bank_no(value.first);
    auto lock = metall_utility::mutex::mutex_lock<k_num_banks>(bank_no);
    return m_banked_map[bank_no].insert(std::forward<value_type>(value)).second;
  }

  std::pair<mapped_type &, std::unique_lock<std::mutex>>
  scoped_edit(const key_type &key) {
    const auto bank_no = calc_bank_no(key);
    auto lock = metall_utility::mutex::mutex_lock<k_num_banks>(bank_no);
    if (!count(key)) {
      [[maybe_unused]] const bool ret = register_key_no_lock(key);
      assert(ret);
    }
    return std::make_pair(std::ref(m_banked_map[bank_no].at(key)), std::move(lock));
  }

  void edit(const key_type &key, const std::function<void(mapped_type &value)> &editor) {
    const auto bank_no = calc_bank_no(key);
    auto lock = metall_utility::mutex::mutex_lock<k_num_banks>(bank_no);
    if (!count(key)) {
      [[maybe_unused]] const bool ret = register_key_no_lock(key);
      assert(ret);
    }
    editor(m_banked_map[bank_no].at(key));
  }

  // ---------------------------------------- Iterator ---------------------------------------- //
  const_iterator cbegin() const {
    return const_iterator(m_banked_map.cbegin(), m_banked_map.cend());
  }

  const_iterator cend() const {
    return const_iterator(m_banked_map.cend(), m_banked_map.cend());
  }

  // ---------------------------------------- Iterator ---------------------------------------- //
  const_iterator find(const key_type &key) const {
    const auto bank_no = calc_bank_no(key);
    auto itr = m_banked_map[bank_no].find(key);
    if (itr != m_banked_map[bank_no].end()) {
      return const_iterator(m_banked_map.cbegin(), itr, m_banked_map.cend());
    }
    return cend();
  }

 private:
  static uint64_t calc_bank_no(const key_type &key) {
    return _bank_no_hasher()(key) % k_num_banks;
  }

  // TODO
  bool register_key_no_lock(const key_type &key) {
    const auto bank_no = calc_bank_no(key);
    return m_banked_map[bank_no].try_emplace(key).second;
  }

  banked_map_type m_banked_map;
};

} // namespace metall::container

#endif //METALL_COTAINER_CONCURRENT_MAP_HPP
