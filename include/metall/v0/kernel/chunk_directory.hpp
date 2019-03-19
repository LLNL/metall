// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_V0_CHUNK_DIRECTORY_HPP
#define METALL_DETAIL_V0_CHUNK_DIRECTORY_HPP

#include <iostream>
#include <vector>
#include <limits>
#include <fstream>
#include <cassert>
#include <metall/detail/utility/common.hpp>
#include <metall/detail/utility/mmap.hpp>
#include <metall/v0/kernel/multilayer_bitset.hpp>
#include <metall/v0/kernel/bin_number_manager.hpp>
#include <metall/v0/kernel/object_size_manager.hpp>

namespace metall {
namespace v0 {
namespace kernel {
namespace {
namespace util = metall::detail::utility;
}

template <typename _chunk_no_type, std::size_t _k_chunk_size, std::size_t _k_max_size>
class chunk_directory {
 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  static constexpr std::size_t k_chunk_size = _k_chunk_size;
  static constexpr std::size_t k_max_size = _k_max_size;
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  static constexpr std::size_t k_num_max_slots = k_chunk_size / bin_no_mngr::to_object_size(0);

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using chunk_no_type = _chunk_no_type;
  using bin_no_type = typename bin_no_mngr::bin_no_type;

 private:
  // -------------------------------------------------------------------------------- //
  // Private types and static values
  // -------------------------------------------------------------------------------- //
  using slot_count_type = typename util::unsigned_variable_type<k_num_max_slots>::type;

  enum chunk_type : uint8_t {
    empty = 0, // This must be 0
    small_chunk = 1,
    large_chunk_head = 2,
    large_chunk_tail = 3
  };

  struct entry_type {
   public:
    entry_type()
        : bin_no(),
          type(chunk_type::empty),
          num_occupied_slots(0),
          slot_occupancy() {}

    bin_no_type bin_no; // 1B
    chunk_type type;    // 1B
    slot_count_type num_occupied_slots; // Could be removed, 4B
    multilayer_bitset slot_occupancy; // 8B
  };

 public:
  // -------------------------------------------------------------------------------- //
  // Public types and static values
  // -------------------------------------------------------------------------------- //
  using slot_no_type = typename util::unsigned_variable_type<k_num_max_slots - 1>::type;

  // -------------------------------------------------------------------------------- //
  // Constructor & assign operator
  // -------------------------------------------------------------------------------- //
  chunk_directory()
      : m_table(nullptr),
        m_num_chunks(0) {}

  ~chunk_directory() {
    for (chunk_no_type chunk_no = 0; chunk_no < m_num_chunks; ++chunk_no) {
      erase(chunk_no);
    }
    deallocate_table();
  }

  chunk_directory(const chunk_directory &) = delete;
  chunk_directory &operator=(const chunk_directory &) = delete;

  chunk_directory(chunk_directory &&) noexcept = default;
  chunk_directory &operator=(chunk_directory &&) noexcept = default;

  /// -------------------------------------------------------------------------------- ///
  /// Public methods
  /// -------------------------------------------------------------------------------- ///
  /// \brief
  /// \param num_chunks
  void initialize(const std::size_t num_chunks) {
    assert(!m_table);
    m_num_chunks = num_chunks;
    allocate_table();
  }

  /// \brief
  /// \param bin_no
  /// \param num_slots
  /// \return
  chunk_no_type insert_small_chunk(const bin_no_type bin_no) {
    const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(bin_no);
    assert(num_slots > 1);

    for (chunk_no_type chunk_no = 0; chunk_no < m_num_chunks; ++chunk_no) {
      if (m_table[chunk_no].type == chunk_type::empty) {
        m_table[chunk_no].bin_no = bin_no;
        m_table[chunk_no].type = chunk_type::small_chunk;
        m_table[chunk_no].num_occupied_slots = 0;
        m_table[chunk_no].slot_occupancy.allocate(num_slots);
        return chunk_no;
      }
    }
    std::cerr << "All chunks are occupied" << std::endl;
    std::abort();
  }

  /// \brief
  /// \param bin_no
  /// \return
  chunk_no_type insert_large_chunk(const bin_no_type bin_no) {
    const std::size_t num_chunks = (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1)  / k_chunk_size;
    assert(num_chunks >= 1);

    chunk_no_type count_empty_chunks = 0;
    for (chunk_no_type chunk_no = 0; chunk_no < m_num_chunks; ++chunk_no) {
      if (m_table[chunk_no].type != chunk_type::empty) {
        count_empty_chunks = 0;
        continue;
      }
      ++count_empty_chunks;
      if (count_empty_chunks == num_chunks) {
        const chunk_no_type top_chunk_no = chunk_no - (count_empty_chunks - 1);
        m_table[top_chunk_no].bin_no = bin_no;
        m_table[top_chunk_no].type = chunk_type::large_chunk_head;

        for (chunk_no_type offset = 1; offset < num_chunks; ++offset) {
          m_table[top_chunk_no + offset].bin_no = bin_no; // just in case
          m_table[top_chunk_no + offset].type = chunk_type::large_chunk_tail;
        }
        return top_chunk_no;
      }
    }

    std::cerr << "Do not have enough chunks" << std::endl;
    std::abort();
  }

  /// \brief
  /// \param chunk_no
  /// \return
  slot_no_type find_and_mark_slot(const chunk_no_type chunk_no) {
    assert(chunk_no < m_num_chunks);
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(m_table[chunk_no].bin_no);
    assert(num_slots >= 1);

    assert(m_table[chunk_no].num_occupied_slots < num_slots);
    const auto empty_slot_no = m_table[chunk_no].slot_occupancy.find_and_set(num_slots);
    assert(empty_slot_no >= 0);
    ++m_table[chunk_no].num_occupied_slots;

    return empty_slot_no;
  }

  /// \brief
  /// \param chunk_no
  /// \param slot_no
  void unmark_slot(const chunk_no_type chunk_no, const slot_no_type slot_no) {
    assert(chunk_no < m_num_chunks);
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(m_table[chunk_no].bin_no);
    assert(num_slots >= 1);

    assert(m_table[chunk_no].num_occupied_slots > 0);
    m_table[chunk_no].slot_occupancy.reset(num_slots, slot_no);
    --m_table[chunk_no].num_occupied_slots;
  }

  /// \brief
  /// \param chunk_no
  /// \return
  bool full_slot(const chunk_no_type chunk_no) const {
    assert(chunk_no < m_num_chunks);
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(m_table[chunk_no].bin_no);
    assert(num_slots >= 1);

    return (m_table[chunk_no].num_occupied_slots == num_slots);
  }

  /// \brief
  /// \param chunk_no
  /// \return
  bool empty_slot(const chunk_no_type chunk_no) const {
    assert(chunk_no < m_num_chunks);
    assert(m_table[chunk_no].type == chunk_type::small_chunk);
    return (m_table[chunk_no].num_occupied_slots == 0);
  }

  /// \brief
  /// \param chunk_no
  void erase(const chunk_no_type chunk_no) {
    assert(chunk_no < m_num_chunks);
    if (m_table[chunk_no].type == chunk_type::empty) return;

    if (m_table[chunk_no].type == chunk_type::small_chunk) {
      const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(m_table[chunk_no].bin_no);
      m_table[chunk_no].type = chunk_type::empty;
      m_table[chunk_no].num_occupied_slots = 0;
      m_table[chunk_no].slot_occupancy.free(num_slots);
    } else {
      m_table[chunk_no].type = chunk_type::empty;
      for (chunk_no_type i = 1;
           chunk_no + i < m_num_chunks && m_table[chunk_no + i].type == chunk_type::large_chunk_tail; ++i) {
        m_table[chunk_no + i].type = chunk_type::empty;
      }
    }
  }

  /// \brief
  /// \param chunk_no
  /// \return
  bin_no_type bin_no(const chunk_no_type chunk_no) const {
    return m_table[chunk_no].bin_no;
  }

  /// \brief
  /// \param path
  bool serialize(const char *path) const {
    std::ofstream ofs(path);
    if (!ofs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    for (chunk_no_type chunk_no = 0; chunk_no < m_num_chunks; ++chunk_no) {
      if (m_table[chunk_no].type == chunk_type::empty) {
        continue;
      }

      ofs << static_cast<uint64_t>(chunk_no)
          << " " << static_cast<uint64_t>(m_table[chunk_no].bin_no)
          << " " << static_cast<uint64_t>(m_table[chunk_no].type);
      if (!ofs) std::cerr << "Something happened in the ofstream: " << path << std::endl;

      if (m_table[chunk_no].type == chunk_type::small_chunk) {
        const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(m_table[chunk_no].bin_no);
        ofs << " " << static_cast<uint64_t>(m_table[chunk_no].num_occupied_slots)
            << " " << m_table[chunk_no].slot_occupancy.serialize(num_slots) << "\n";
        if (!ofs) std::cerr << "Something happened in the ofstream: " << path << std::endl;

      } else if (m_table[chunk_no].type == chunk_type::large_chunk_head
          || m_table[chunk_no].type == chunk_type::large_chunk_tail) {
        ofs << "\n";
        if (!ofs) std::cerr << "Something happened in the ofstream: " << path << std::endl;

      } else {
        std::cerr << "Unexpected chunk status " << std::endl;
        std::abort();
      }
    }

    ofs.close();

    return true;
  }

  /// \brief
  /// \param path
  /// \return
  bool deserialize(const char *path) {
    std::ifstream ifs(path);
    if (!ifs.is_open()) {
      std::cerr << "Cannot open: " << path << std::endl;
      return false;
    }

    uint64_t buf1;
    uint64_t buf2;
    uint64_t buf3;
    while (ifs >> buf1 >> buf2 >> buf3) {
      const auto chunk_no = static_cast<chunk_no_type>(buf1);
      const auto bin_no = static_cast<bin_no_type>(buf2);
      m_table[chunk_no].bin_no = bin_no;

      using status_underlying_type = typename std::underlying_type<chunk_type>::type;
      const auto type = static_cast<status_underlying_type>(buf3);
      if (type == static_cast<status_underlying_type>(chunk_type::small_chunk)) {
        m_table[chunk_no].type = chunk_type::small_chunk;
      } else if (type == static_cast<status_underlying_type>(chunk_type::large_chunk_head)) {
        m_table[chunk_no].type = chunk_type::large_chunk_head;
      } else if (type == static_cast<status_underlying_type>(chunk_type::large_chunk_tail)) {
        m_table[chunk_no].type = chunk_type::large_chunk_tail;
      } else {
        std::cerr << "Invalid chunk type" << std::endl;
        return false;
      }

      if (m_table[chunk_no].type == chunk_type::small_chunk) {
        const slot_count_type num_slots = k_chunk_size / bin_no_mngr::to_object_size(bin_no);
        if (!(ifs >> buf1)) {
          std::cerr << "Cannot read a file: " << path << std::endl;
          std::abort();
        }
        if (num_slots < buf1) {
          std::cerr << "Invalid num_occupied_slots: " << buf1 << std::endl;
          std::abort();
        }
        m_table[chunk_no].num_occupied_slots = buf1;

        std::string bitset_buf;
        std::getline(ifs, bitset_buf);
        if (bitset_buf.empty() || bitset_buf[0] != ' ') {
          std::cerr << "Invalid input for slot_occupancy: " << bitset_buf << std::endl;
          std::abort();
        }
        bitset_buf.erase(0, 1);

        m_table[chunk_no].slot_occupancy.allocate(num_slots);
        if (!m_table[chunk_no].slot_occupancy.deserialize(num_slots, bitset_buf)) {
          std::cerr << "Invalid input for slot_occupancy: " << bitset_buf << std::endl;
          std::abort();
        }
      }
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
  void allocate_table() {
    /// CAUTION: Assumes that mmap + MAP_ANONYMOUS returns zero-initialized region
    m_table = static_cast<entry_type *>(util::os_mmap(nullptr, m_num_chunks * sizeof(entry_type),
                                                      PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (!m_table) {
      std::cerr << "Cannot allocate chunk table" << std::endl;
      std::abort();
    }
  }

  void deallocate_table() {
    util::os_munmap(m_table, m_num_chunks * sizeof(entry_type));
  }

  /// -------------------------------------------------------------------------------- ///
  /// Private fields
  /// -------------------------------------------------------------------------------- ///
  entry_type *m_table;
  std::size_t m_num_chunks;
};

} // namespace kernel
} // namespace v0
} // namespace metall

#endif //METALL_DETAIL_V0_CHUNK_DIRECTORY_HPP
