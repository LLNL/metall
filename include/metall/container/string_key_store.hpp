// Copyright 2022 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_CONTAINER_CONCURRENT_STRING_KEY_STORE_HPP
#define METALL_CONTAINER_CONCURRENT_STRING_KEY_STORE_HPP

#include <memory>
#include <string_view>
#include <utility>
#include <tuple>
#include <metall/container/unordered_map.hpp>
#include <metall/container/string.hpp>
#include <metall/container/scoped_allocator.hpp>
#include <metall/container/string_key_store_locator.hpp>
#include <metall/metall.hpp>

namespace metall::container {

namespace {
namespace mc = metall::container;
}

/// \brief A ke-value store that uses string for its key.
/// \warning This container is designed to work as the top-level container,
/// i.e., it does not work if used inside another container.
/// \tparam _value_type A value type.
/// \tparam allocator_type An allocator type.
template <typename _value_type, typename allocator_type = metall::manager::allocator_type<std::byte>>
class string_key_store {
 private:
  template <typename T>
  using other_allocator = typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;

  template <typename T>
  using other_scoped_allocator = mc::scoped_allocator_adaptor<other_allocator<T>>;

  using internal_id_type = uint64_t;
  using internal_string_type = mc::basic_string<char, std::char_traits<char>, other_allocator<char>>;
  using internal_value_type = std::tuple<internal_string_type, _value_type>;

  using map_type = mc::unordered_multimap<internal_id_type,
                                          internal_value_type,
                                          std::hash<internal_id_type>,
                                          std::equal_to<internal_id_type>,
                                          other_scoped_allocator<std::pair<const internal_id_type,
                                                                           internal_value_type>>>;

  /// Used for representing 'invalid key'.
  static constexpr internal_id_type k_max_internal_id = std::numeric_limits<internal_id_type>::max();

 public:
  using key_type = std::string_view;
  using value_type = _value_type;
  using locator_type = string_key_store_locator<typename map_type::const_iterator>;

  /// \brief Constructor.
  /// \param allocator An allocator object.
  explicit string_key_store(const allocator_type &allocator = allocator_type())
      : m_map(allocator) {}

  /// \brief Constructor.
  /// \param unique Accept duplicate keys if false is specified.
  /// \param hash_seed Hash function seed.
  /// \param allocator An allocator object.
  string_key_store(const bool unique,
                   const uint64_t hash_seed,
                   const allocator_type &allocator = allocator_type())
      : m_unique(unique),
        m_hash_seed(hash_seed),
        m_map(allocator) {}

  /// \brief Copy constructor
  string_key_store(const string_key_store &) = default;

  /// \brief Allocator-extended copy constructor
  string_key_store(const string_key_store &other, const allocator_type &alloc)
      : m_unique(other.m_unique),
        m_hash_seed(other.m_hash_seed),
        m_map(other.m_map, alloc) {}

  /// \brief Move constructor
  string_key_store(string_key_store &&) noexcept = default;

  /// \brief Allocator-extended move constructor
  string_key_store(string_key_store &&other, const allocator_type &alloc) noexcept
      : m_unique(other.m_unique),
        m_hash_seed(other.m_hash_seed),
        m_map(std::move(other.m_map), alloc) {}

  /// \brief Copy assignment operator
  string_key_store &operator=(const string_key_store &) = default;

  /// \brief Move assignment operator
  string_key_store &operator=(string_key_store &&) noexcept = default;

  /// \brief Inserts a key with the default value.
  /// If the unique parameter in the constructor was set to true and a duplicate key item already exists,
  /// this function does nothing and returns false.
  /// \param key A key to insert.
  /// \return True if an item is inserted; otherwise, false.
  bool insert(const key_type &key) {
    const auto internal_id = priv_find_or_generate_internal_id(key);
    if (m_unique && m_map.count(internal_id) == 1) {
      return false;
    }

    // We delegate tuple to decide if value_type's constructor needs an allocator object.
    // By taking this approach, we can avoid the complex logic of uses-allocator construction.
    // This is why we use tuple for internal_value_type.
    auto internal_value = internal_value_type{std::allocator_arg, m_map.get_allocator()};
    std::get<0>(internal_value) = key;

    m_map.emplace(internal_id, std::move(internal_value));

    return true;
  }

  /// \brief Inserts an item.
  /// Suppose the unique parameter was set to true in the constructor and a duplicate key item exists.
  /// In that case, this function just updates the value of the existing one.
  /// \param key A key to insert.
  /// \param value A value to insert.
  /// \return Always true.
  bool insert(const key_type &key, const value_type &value) {
    const auto internal_id = priv_find_or_generate_internal_id(key);

    assert(!m_unique || m_map.count(internal_id) <= 1);
    if (m_unique && m_map.count(internal_id) == 1) {
      assert(m_map.count(internal_id) == 1);
      std::get<1>(m_map.find(internal_id)->second) = value;
    } else {
      m_map.emplace(internal_id, internal_value_type{std::allocator_arg, m_map.get_allocator(), key.data(), value});
    }
    return true;
  }

  /// \brief Insert() with move operator version.
  /// \param key A key to insert.
  /// \param value A value to insert.
  /// \return Always true.
  bool insert(const key_type &key, value_type &&value) {
    const auto internal_id = priv_find_or_generate_internal_id(key);

    assert(!m_unique || m_map.count(internal_id) <= 1);
    if (m_unique && m_map.count(internal_id) == 1) {
      std::get<1>(m_map.find(internal_id)->second) = std::move(value);
    } else {
      m_map.emplace(internal_id,
                    internal_value_type{std::allocator_arg, m_map.get_allocator(), key.data(), std::move(value)});
    }
    return true;
  }

  /// \brief Clear all contents. This call does not reduce the memory usage.
  void clear() {
    m_map.clear();
    m_max_id_probe_distance = 0;
  }

  /// \brief Counts the number of items associated with the key.
  /// \param key A key to count.
  /// \return The number of items associated with the key.
  std::size_t count(const key_type &key) const {
    const auto internal_id = priv_find_internal_id(key);
    return m_map.count(internal_id);
  }

  /// \brief Returns the number of elements in this container.
  /// \return The number of elements in this container.
  std::size_t size() const {
    return m_map.size();
  }

  /// \brief Returns the key of the element at 'position'.
  /// \param position A locator object.
  /// \return The key of the element at 'position'.
  const key_type key(const locator_type &position) const {
    const auto &key = std::get<0>(position.m_iterator->second);
    return key_type(key.c_str());
  }

  /// \brief Returns the value of the element at 'position'.
  /// \param position A locator object.
  /// \return The value of the element at 'position'.
  value_type &value(const locator_type &position) {
    // Convert to a non-const iterator
    auto non_const_iterator = m_map.erase(position.m_iterator, position.m_iterator);
    return std::get<1>(non_const_iterator->second);
  }

  /// \brief Returns the value of the element at 'position'.
  /// \param position A locator object.
  /// \return The value of the element at 'position' as const.
  const value_type &value(const locator_type &position) const {
    return std::get<1>(position.m_iterator->second);
  }

  /// \brief Finds an element with key equivalent to 'key'.
  /// \param key The key of an element to find.
  /// \return An locator object that points the found element.
  locator_type find(const key_type &key) const {
    const auto internal_id = priv_find_internal_id(key);
    return locator_type(m_map.find(internal_id));
  }

  /// \brief Returns a range containing all elements with key key in the container.
  /// \param key The key of elements to find.
  /// \return A pair of locator objects.
  /// The range is defined by two locators.
  /// The first points to the first element of the range,
  /// and the second points to the element following the last element of the range.
  std::pair<locator_type, locator_type> equal_range(const key_type &key) const {
    const auto internal_id = priv_find_internal_id(key);
    const auto range = m_map.equal_range(internal_id);
    return std::make_pair(locator_type(range.first), locator_type(range.second));
  }

  /// \brief Return an iterator that points the first element in the container.
  /// \return An iterator that points the first element in the container.
  locator_type begin() const {
    return locator_type(m_map.begin());
  }

  /// \brief Returns an iterator to the element following the last element.
  /// \return An iterator to the element following the last element.
  locator_type end() const {
    return locator_type(m_map.end());
  }

  /// \brief Removes all elements with the key equivalent to key.
  /// \param key The key of elements to remove.
  /// \return The number of elements removed.
  std::size_t erase(const key_type &key) {
    const auto internal_id = priv_find_internal_id(key);
    return m_map.erase(internal_id);
  }

  /// \brief Removes the element at 'position'.
  /// \param position The position of an element to remove.
  /// \return A locator that points to the next element of the removed one.
  locator_type erase(const locator_type &position) {
    if (position == end()) return end();
    return locator_type(m_map.erase(position.m_iterator));
  }

  /// \brief Returns the maximum ID probe distance.
  /// In other words, the maximum number of key pairs that have the same hash value.
  /// \return The maximum ID probe distance.
  std::size_t max_id_probe_distance() const {
    return m_max_id_probe_distance;
  }

  /// \brief Rehash elements.
  void rehash() {
    auto old_map = map_type(get_allocator());
    std::swap(old_map, m_map);
    m_map.clear();
    assert(m_map.empty());
    for (auto &elem: old_map) {
      insert(std::get<0>(elem.second), std::move(std::get<1>(elem.second)));
    }
  }

  /// \brief Returns an instance of the internal allocator.
  /// \return An instance of the internal allocator.
  allocator_type get_allocator() {
    return m_map.get_allocator();
  }

  /// \brief Returns if this container inserts keys uniquely.
  /// \return True if this container inserts key avoiding duplicates; otherwise false.
  bool unique() const {
    return m_unique;
  }

  /// \brief Returns the hash seed.
  /// \return Hash seed.
  bool hash_seed() const {
    return m_hash_seed;
  }

 private:
  /// \brief Generates a new internal ID for 'key'.
  internal_id_type priv_generate_internal_id(const key_type &key) {
    auto internal_id = priv_hash_key(key, m_hash_seed);

    std::size_t distance = 0;
    while (m_map.count(internal_id) > 0) {
      internal_id = priv_increment_internal_id(internal_id);
      ++distance;
    }
    m_max_id_probe_distance = std::max(distance, m_max_id_probe_distance);

    return internal_id;
  }

  /// \brief Finds the internal ID that corresponds with 'key'.
  /// If this container does not have an element with 'key',
  /// returns k_max_internal_id.
  internal_id_type priv_find_internal_id(const key_type &key) const {
    auto internal_id = priv_hash_key(key, m_hash_seed);

    for (std::size_t d = 0; d <= m_max_id_probe_distance; ++d) {
      const auto itr = m_map.find(internal_id);
      if (itr == m_map.end()) {
        break;
      }

      if (std::get<0>(itr->second) == key) {
        return internal_id;
      }
      internal_id = priv_increment_internal_id(internal_id);
    }

    return k_max_internal_id; // Couldn't find
  }

  /// \brief Finds the internal ID that corresponds to 'key'.
  /// If this container does not have an element with 'key',
  /// Generate a new internal ID.
  internal_id_type priv_find_or_generate_internal_id(const key_type &key) {
    auto internal_id = priv_find_internal_id(key);
    if (internal_id == k_max_internal_id) { // Couldn't find
      // Generate a new one.
      internal_id = priv_generate_internal_id(key);
    }
    return internal_id;
  }

  static internal_id_type priv_hash_key(const key_type &key, [[maybe_unused]]const uint64_t seed) {
#ifdef METALL_CONTAINER_STRING_KEY_STORE_USE_SIMPLE_HASH
    internal_id_type hash = key.empty() ? 0 : (uint8_t)key[0] % 2;
#else
    auto hash = (internal_id_type)metall::mtlldetail::MurmurHash64A(key.data(), (int)key.length(), seed);
#endif
    if (hash == k_max_internal_id) {
      hash = priv_increment_internal_id(hash);
    }
    assert(hash != k_max_internal_id);
    return hash;
  }

  static internal_id_type priv_increment_internal_id(const internal_id_type id) {
    const auto new_id = (id + 1) % k_max_internal_id;
    assert(new_id != k_max_internal_id);
    return new_id;
  }

  bool m_unique{false};
  uint64_t m_hash_seed{123};
  map_type m_map{allocator_type{}};
  std::size_t m_max_id_probe_distance{0};
};

} // namespace metalldata

#endif //METALL_CONTAINER_CONCURRENT_STRING_KEY_STORE_HPP
