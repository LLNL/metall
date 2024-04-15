// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_BIN_DIRECTORY_HPP
#define METALL_DETAIL_BIN_DIRECTORY_HPP

#include <iostream>
#include <limits>
#include <fstream>
#include <string>
#include <sstream>
#include <cassert>
#include <functional>
#include <memory>
#include <filesystem>

#include <boost/container/vector.hpp>
#include <boost/container/scoped_allocator.hpp>

#ifdef METALL_USE_SORTED_BIN
#include <boost/container/flat_set.hpp>
#else
#include <boost/container/deque.hpp>
#endif

#include <metall/detail/utilities.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {

namespace {
namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;
}

/// \brief A simple key-value store designed to store values related to memory
/// address, such as free chunk numbers or free objects. Values are sorted with
/// ascending order if METALL_USE_SORTED_BIN is defined; otherwise, values are
/// stored in the LIFO order. \tparam _k_num_bins The number of bins \tparam
/// _value_type The value type to store \tparam _allocator_type The allocator
/// type to allocate internal data
template <std::size_t _k_num_bins, typename _value_type,
          typename _allocator_type = std::allocator<std::byte>>
class bin_directory {
 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  static constexpr std::size_t k_num_bins = _k_num_bins;
  using value_type = _value_type;
  using allocator_type = _allocator_type;
  using bin_no_type = typename mdtl::unsigned_variable_type<k_num_bins>::type;

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //
  template <typename T>
  using other_allocator_type =
      typename std::allocator_traits<allocator_type>::template rebind_alloc<T>;
#ifdef METALL_USE_SORTED_BIN
  using bin_allocator_type = other_allocator_type<value_type>;
  // Sort values with descending order internally to simplify implementation
  using bin_type =
      boost::container::flat_set<value_type, std::greater<value_type>,
                                 bin_allocator_type>;
#else
  using bin_allocator_type = other_allocator_type<value_type>;
  using bin_type = boost::container::deque<value_type, bin_allocator_type>;
#endif
  using table_allocator = boost::container::scoped_allocator_adaptor<
      other_allocator_type<bin_type>>;
  using table_type = boost::container::vector<bin_type, table_allocator>;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using const_bin_iterator = typename bin_type::const_iterator;

  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  explicit bin_directory(const allocator_type &allocator = allocator_type())
      : m_table(k_num_bins, bin_type(allocator), allocator) {}

  ~bin_directory() noexcept = default;
  bin_directory(const bin_directory &) = default;
  bin_directory(bin_directory &&) noexcept = default;
  bin_directory &operator=(const bin_directory &) = default;
  bin_directory &operator=(bin_directory &&) noexcept = default;

  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief
  /// \param bin_no
  /// \return
  bool empty(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].empty();
  }

  /// \brief
  /// \param bin_no
  /// \return
  std::size_t size(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].size();
  }

  /// \brief
  /// \param bin_no
  /// \return
  value_type front(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    assert(!empty(bin_no));
#ifdef METALL_USE_SORTED_BIN
    return *(m_table[bin_no].end() - 1);
#else
    return m_table[bin_no].front();
#endif
  }

  /// \brief
  /// \param bin_no
  /// \param value
  void insert(const bin_no_type bin_no, const value_type value) {
    assert(bin_no < k_num_bins);
#ifdef METALL_USE_SORTED_BIN
    m_table[bin_no].insert(value);
#else
    m_table[bin_no].emplace_front(value);
#endif
  }

  /// \brief
  /// \param bin_no
  void pop(const bin_no_type bin_no) {
    assert(bin_no < k_num_bins);
#ifdef METALL_USE_SORTED_BIN
    m_table[bin_no].erase(m_table[bin_no].end() - 1);
#else
    m_table[bin_no].pop_front();
#endif
  }

  /// \brief
  /// \param bin_no
  /// \param value
  /// \return
  bool erase(const bin_no_type bin_no, const value_type value) {
    assert(bin_no < k_num_bins);
#ifdef METALL_USE_SORTED_BIN
    const auto itr = m_table[bin_no].find(value);
    if (itr != m_table[bin_no].end()) {
      m_table[bin_no].erase(itr);
      return true;
    }
#else
    for (auto itr = m_table[bin_no].begin(), end = m_table[bin_no].end();
         itr != end; ++itr) {
      if (*itr == value) {
        m_table[bin_no].erase(itr);
        return true;
      }
    }
#endif
    return false;
  }

  /// \brief
  void clear() {
    for (uint64_t i = 0; i < m_table.size(); ++i) {
      m_table[i].clear();
    }
  }

  /// \brief
  /// \param bin_no
  /// \return
  const_bin_iterator begin(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].begin();
  }

  /// \brief
  /// \param bin_no
  /// \return
  const_bin_iterator end(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].end();
  }

  /// \brief
  /// \param path
  bool serialize(const fs::path &path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      std::stringstream ss;
      ss << "Cannot open: " << path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    for (uint64_t i = 0; i < m_table.size(); ++i) {
      for (const auto value : m_table[i]) {
        ofs << static_cast<uint64_t>(i) << " " << static_cast<uint64_t>(value)
            << "\n";
        if (!ofs) {
          std::stringstream ss;
          ss << "Something happened in the ofstream: " << path;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }
      }
    }
    ofs.close();

    return true;
  }

  /// \brief
  /// \param path
  bool deserialize(const fs::path &path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      std::stringstream ss;
      ss << "Cannot open: " << path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    uint64_t buf1;
    uint64_t buf2;
    while (ifs >> buf1 >> buf2) {
      const auto bin_no = static_cast<bin_no_type>(buf1);
      const auto value = static_cast<value_type>(buf2);

      if (m_table.size() <= bin_no) {
        std::stringstream ss;
        ss << "Too large bin number is found: " << bin_no;
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
        return false;
      }
#ifdef METALL_USE_SORTED_BIN
      m_table[bin_no].insert(value);
#else
      m_table[bin_no].emplace_back(value);
#endif
    }

    if (!ifs.eof()) {
      std::stringstream ss;
      ss << "Something happened in the ifstream: " << path;
      logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
      return false;
    }

    ifs.close();

    return true;
  }

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //

  // -------------------- //
  // Private methods
  // -------------------- //

  // -------------------- //
  // Private fields
  // -------------------- //
  table_type m_table;
};

}  // namespace kernel
}  // namespace metall

#endif  // METALL_DETAIL_BIN_DIRECTORY_HPP
