// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_BIN_DIRECTORY_HPP
#define METALL_DETAIL_V0_BIN_DIRECTORY_HPP

#include <iostream>
#include <limits>
#include <fstream>
#include <string>
#include <array>
#include <cassert>
#include <functional>

#define USE_SPACE_AWARE_BIN

#ifdef USE_SPACE_AWARE_BIN
#include <boost/container/flat_set.hpp>
#else
#include <deque>
#endif

#include <metall/detail/utility/common.hpp>

namespace metall {
namespace v0 {
namespace kernel {

namespace {
namespace util = metall::detail::utility;
}

/// \brief
/// \tparam _k_num_bins
/// \tparam _chunk_no_type
template <std::size_t _k_num_bins, typename _chunk_no_type>
class bin_directory {
 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  static constexpr std::size_t k_num_bins = _k_num_bins;
  using chunk_no_type = _chunk_no_type;
  using bin_no_type = typename util::unsigned_variable_type<k_num_bins>::type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
#ifdef USE_SPACE_AWARE_BIN
  using bin_type = boost::container::flat_set<chunk_no_type, std::greater<chunk_no_type>>;
#else
  using bin_type = std::deque<chunk_no_type>;
#endif
  using table_type = std::array<bin_type, k_num_bins>;

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using bin_const_iterator = typename bin_type::const_iterator;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  bin_directory() = default;
  ~bin_directory() = default;
  bin_directory(const bin_directory &) = default;
  bin_directory(bin_directory &&) noexcept = default;
  bin_directory &operator=(const bin_directory &) = default;
  bin_directory &operator=(bin_directory &&) noexcept = default;

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
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
  chunk_no_type front(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    assert(!empty(bin_no));
#ifdef USE_SPACE_AWARE_BIN
    return *(m_table[bin_no].end() - 1);
#else
    return m_table[bin_no].front();
#endif
  }

  /// \brief
  /// \param bin_no
  /// \param chunk_no
  void insert(const bin_no_type bin_no, const chunk_no_type chunk_no) {
    assert(bin_no < k_num_bins);
#ifdef USE_SPACE_AWARE_BIN
    m_table[bin_no].insert(chunk_no);
#else
    m_table[bin_no].emplace_front(chunk_no);
#endif
  }

  /// \brief
  /// \param bin_no
  void pop(const bin_no_type bin_no) {
    assert(bin_no < k_num_bins);
#ifdef USE_SPACE_AWARE_BIN
    m_table[bin_no].erase(m_table[bin_no].end() - 1);
#else
    m_table[bin_no].pop_front();
#endif
  }

  /// \brief
  /// \param bin_no
  /// \param chunk_no
  /// \return
  bool erase(const bin_no_type bin_no, const chunk_no_type chunk_no) {
    assert(bin_no < k_num_bins);
#ifdef USE_SPACE_AWARE_BIN
    const auto itr = m_table[bin_no].find(chunk_no);
    if (itr != m_table[bin_no].end()) {
      m_table[bin_no].erase(itr);
      return true;
    }
#else
    for (auto itr = m_table[bin_no].begin(), end = m_table[bin_no].end(); itr != end; ++itr) {
      if (*itr == chunk_no) {
        m_table[bin_no].erase(itr);
        return true;
      }
    }
#endif
    return false;
  }

  bin_const_iterator begin(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].begin();
  }

  bin_const_iterator end(const bin_no_type bin_no) const {
    assert(bin_no < k_num_bins);
    return m_table[bin_no].end();
  }

  /// \brief
  /// \param path
  bool serialize(const char *path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    for (uint64_t i = 0; i < m_table.size(); ++i) {
      for (const auto chunk_no : m_table[i]) {
        ofs << static_cast<uint64_t>(i) << " " << static_cast<uint64_t>(chunk_no) << "\n";
        if (!ofs) {
          std::cerr << "Something happened in the ofstream: " << path << std::endl;
          return false;
        }
      }
    }
    ofs.close();

    return true;
  }

  /// \brief
  /// \param path
  bool deserialize(const char *path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    uint64_t buf1;
    uint64_t buf2;
    while (ifs >> buf1 >> buf2) {
      const auto bin_no = static_cast<bin_no_type>(buf1);
      const auto chunk_no = static_cast<chunk_no_type>(buf2);

      if (m_table.size() <= bin_no) {
        std::cerr << "Too large bin number is found: " << bin_no << std::endl;
        return false;
      }
      insert(bin_no, chunk_no);
    }

    if (!ifs.eof()) {
      std::cerr << "Something happened in the ifstream: " << path << std::endl;
      return false;
    }

    ifs.close();

    return true;
  }

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //

  /// -------------------------------------------------------------------------------- ///
  /// Private methods
  /// -------------------------------------------------------------------------------- ///

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  table_type m_table;
};

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_BIN_DIRECTORY_HPP
