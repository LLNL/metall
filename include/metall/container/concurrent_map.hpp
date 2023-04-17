// Copyright 2020 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_CONCURRENT_MAP_HPP
#define METALL_CONTAINER_CONCURRENT_MAP_HPP

#include <functional>
#include <memory>
#include <boost/container/vector.hpp>
#include <boost/container/map.hpp>
#include <boost/container/scoped_allocator.hpp>
#include <metall/utility/mutex.hpp>
#include <metall/utility/container_of_containers_iterator_adaptor.hpp>

/// \namespace metall::container
/// \brief Namespace for Metall container
namespace metall::container {

/// \brief A concurrent map container which can be stored in persistent memory.
/// This container does not allocate mutex objects internally, but allocates
/// them as static objects. To achieve high concurrency, this container
/// allocates multiple banks, where a bank consists of an actual STL map object
/// and a mutex object. \tparam _key_type A key type. \tparam _mapped_type A
/// mapped type. \tparam _compare A key compare. \tparam _bank_no_hasher A key
/// hasher. \tparam _allocator An allocator. \tparam k_num_banks The number of
/// banks to be allocated.
template <typename _key_type, typename _mapped_type,
          typename _compare = std::less<_key_type>,
          typename _bank_no_hasher = std::hash<_key_type>,
          typename _allocator =
              std::allocator<std::pair<const _key_type, _mapped_type>>,
          int k_num_banks = 1024>
class concurrent_map {
 private:
  template <typename T>
  using other_allocator_type =
      typename std::allocator_traits<_allocator>::template rebind_alloc<T>;

  using internal_map_type =
      boost::container::map<_key_type, _mapped_type, _compare, _allocator>;
  using banked_map_allocator_type = other_allocator_type<internal_map_type>;
  using banked_map_type = std::vector<
      internal_map_type,
      boost::container::scoped_allocator_adaptor<banked_map_allocator_type>>;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  /// \brief A key type.
  using key_type = typename internal_map_type::key_type;
  /// \brief A mapped type.
  using mapped_type = typename internal_map_type::mapped_type;
  /// \brief A value type (i.e., std::pair<const key_type, mapped_type>).
  using value_type = typename internal_map_type::value_type;
  /// \brief A unsigned integer type (usually std::size_t).
  using size_type = typename internal_map_type::size_type;
  /// \brief An allocator type.
  using allocator_type = _allocator;

  /// \brief A const iterator type.
  using const_iterator =
      metall::utility::container_of_containers_iterator_adaptor<
          typename banked_map_type::const_iterator,
          typename internal_map_type::const_iterator>;

  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  explicit concurrent_map(const _allocator &allocator = _allocator())
      : m_banked_map(k_num_banks, allocator), m_num_items(0) {}

  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief Returns the number of elements matching specific key.
  /// \param key A key value of the elements to count.
  /// \return The number of elements with key that compares equivalent to key,
  /// which is either 1 or 0.
  size_type count(const key_type &key) const {
    const auto bank_no = calc_bank_no(key);
    return m_banked_map[bank_no].count(key);
  }

  /// \brief Returns the number of elements in the container.
  /// \return The number of elements in the container.
  size_type size() const { return m_num_items; }

  // ---------- Modifier ---------- //
  /// \brief Inserts element into the container
  /// if the container doesn't already contain an element with an equivalent
  /// key. \param value An element value to insert. \return A bool denoting
  /// whether the insertion took place.
  bool insert(value_type &&value) {
    const auto bank_no = calc_bank_no(value.first);
    auto lock = metall::utility::mutex::mutex_lock<k_num_banks>(bank_no);
    const bool ret =
        m_banked_map[bank_no].insert(std::forward<value_type>(value)).second;
    m_num_items += (ret) ? 1 : 0;
    return ret;
  }

  /// \brief Provides a way to edit an element exclusively.
  /// If no element exists with an equivalent key, this container creates a new
  /// element with key. \param key A key of the element to edit. \return A pair
  /// of a reference to the element and a mutex ownership wrapper.
  std::pair<mapped_type &, std::unique_lock<std::mutex>> scoped_edit(
      const key_type &key) {
    const auto bank_no = calc_bank_no(key);
    auto lock = metall::utility::mutex::mutex_lock<k_num_banks>(bank_no);
    if (!count(key)) {
      [[maybe_unused]] const bool ret = register_key_no_lock(key);
      assert(ret);
      ++m_num_items;
    }
    return std::make_pair(std::ref(m_banked_map[bank_no].at(key)),
                          std::move(lock));
  }

  /// \brief Provides a way to edit an element exclusively.
  /// If no element exists with an equivalent key, this container creates a new
  /// element with key. \param key A key of the element to edit. \param editor A
  /// function object which edits an element.
  void edit(const key_type &key,
            const std::function<void(mapped_type &mapped_value)> &editor) {
    const auto bank_no = calc_bank_no(key);
    auto lock = metall::utility::mutex::mutex_lock<k_num_banks>(bank_no);
    if (!count(key)) {
      [[maybe_unused]] const bool ret = register_key_no_lock(key);
      assert(ret);
      ++m_num_items;
    }
    editor(m_banked_map[bank_no].at(key));
  }

  // ---------- Iterator ---------- //
  /// \brief Returns an iterator to the first element of the map.
  /// If the container is empty, the returned iterator will be equal to end().
  /// \return A const iterator to the first element.
  const_iterator cbegin() const {
    return const_iterator(m_banked_map.cbegin(), m_banked_map.cend());
  }

  /// \brief Returns an iterator to the element following the last element of
  /// the map. This element acts as a placeholder; attempting to access it
  /// results in undefined behavior. \return A const iterator to the element
  /// following the last element.
  const_iterator cend() const {
    return const_iterator(m_banked_map.cend(), m_banked_map.cend());
  }

  // ---------- Look up ---------- //
  /// \brief Finds an element with an equivalent key.
  /// \param key A key value of the element to search for
  /// \return An iterator to an element with an equivalent key.
  /// If no such element is found, the returned iterator will be equal to end().
  const_iterator find(const key_type &key) const {
    const auto bank_no = calc_bank_no(key);
    auto itr = m_banked_map[bank_no].find(key);
    if (itr != m_banked_map[bank_no].end()) {
      return const_iterator(m_banked_map.cbegin(), itr, m_banked_map.cend());
    }
    return cend();
  }

  // ---------- Allocator ---------- //
  /// \brief Returns the allocator associated with the container.
  /// \return An object of the associated allocator.
  allocator_type get_allocator() const {
    return allocator_type(m_banked_map.get_allocator());
  }

 private:
  static uint64_t calc_bank_no(const key_type &key) {
    return _bank_no_hasher()(key) % k_num_banks;
  }

  bool register_key_no_lock(const key_type &key) {
    const auto bank_no = calc_bank_no(key);
    return m_banked_map[bank_no].try_emplace(key).second;
  }

  banked_map_type m_banked_map;
  size_type m_num_items;
};

}  // namespace metall::container

/// \example concurrent_map.cpp
/// This is an example of how to use the concurrent_map class with Metall.

#endif  // METALL_CONTAINER_CONCURRENT_MAP_HPP
