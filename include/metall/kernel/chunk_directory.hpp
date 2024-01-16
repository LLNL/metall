// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_CHUNK_DIRECTORY_HPP
#define METALL_DETAIL_CHUNK_DIRECTORY_HPP

#include <limits>
#include <fstream>
#include <cassert>
#include <type_traits>
#include <vector>
#include <filesystem>

#include <metall/detail/utilities.hpp>
#include <metall/detail/mmap.hpp>
#include <metall/kernel/multilayer_bitset.hpp>
#include <metall/kernel/bin_number_manager.hpp>
#include <metall/kernel/object_size_manager.hpp>
#include <metall/logger.hpp>

namespace metall {
namespace kernel {
namespace {
namespace fs = std::filesystem;
namespace mdtl = metall::mtlldetail;
}  // namespace

/// \brief Chunk directory class.
/// Chunk directory is a table that stores information about chunks.
/// This class assumes that race condition is handled by the caller.
template <typename _chunk_no_type, std::size_t _k_chunk_size,
          std::size_t _k_max_size>
class chunk_directory {
 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //
  static constexpr std::size_t k_chunk_size = _k_chunk_size;
  static constexpr std::size_t k_max_size = _k_max_size;
  using bin_no_mngr = bin_number_manager<k_chunk_size, k_max_size>;
  static constexpr std::size_t k_num_max_slots =
      k_chunk_size / bin_no_mngr::to_object_size(0);
  using multilayer_bitset_type = multilayer_bitset;

 public:
  // -------------------- //
  // Public types and static values
  // -------------------- //
  using chunk_no_type = _chunk_no_type;
  using bin_no_type = typename bin_no_mngr::bin_no_type;
  using slot_no_type = multilayer_bitset_type::bit_position_type;
  using slot_count_type =
      typename mdtl::unsigned_variable_type<k_num_max_slots>::type;

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //
  enum chunk_type : uint8_t {
    unused = 0,
    small_chunk = 1,
    large_chunk_head = 2,
    large_chunk_body = 3
  };

  // Chunk directory is just an array of this structure
  struct entry_type {
    void init() {
      type = chunk_type::unused;
      num_occupied_slots = 0;
      slot_occupancy.reset();
    }

    bin_no_type bin_no;                     // 1 byte
    chunk_type type;                        // 1 byte
    slot_count_type num_occupied_slots;     // 4 bytes, just for small chunk
    multilayer_bitset_type slot_occupancy;  // 8 bytes, just for small chunk
  };

 public:
  // -------------------- //
  // Constructor & assign operator
  // -------------------- //
  /// \brief Constructor. This constructor allocates memory for the chunk
  /// directory. \param max_num_chunks Maximum number of chunks that can be
  /// managed.
  explicit chunk_directory(const std::size_t max_num_chunks)
      : m_table(nullptr),
        m_max_num_chunks(max_num_chunks),
        m_last_used_chunk_no(-1) {
    priv_allocate();
  }

  ~chunk_directory() noexcept { priv_destroy(); }

  chunk_directory(const chunk_directory &) = delete;
  chunk_directory &operator=(const chunk_directory &) = delete;

  chunk_directory(chunk_directory &&) noexcept = default;
  chunk_directory &operator=(chunk_directory &&) noexcept = default;

  // -------------------- //
  // Public methods
  // -------------------- //
  /// \brief Registers a new chunk for a bin whose bin number is 'bin_no'.
  /// Requires a global lock to avoid race condition.
  /// \param bin_no Bin number.
  /// \return Returns the chunk number of the new chunk.
  chunk_no_type insert(const bin_no_type bin_no) {
    chunk_no_type inserted_chunk_no;

    if (bin_no < bin_no_mngr::num_small_bins()) {
      inserted_chunk_no = priv_insert_small_chunk(bin_no);
    } else {
      inserted_chunk_no = priv_insert_large_chunk(bin_no);
    }
    assert(inserted_chunk_no < size());

    return inserted_chunk_no;
  }

  /// \brief Erases a chunk whose chunk number is 'chunk_no'.
  /// Requires a global lock to avoid race condition.
  /// \param chunk_no Chunk number to erase.
  void erase(const chunk_no_type chunk_no) {
    assert(chunk_no < size());
    if (unused_chunk(chunk_no)) return;

    if (m_table[chunk_no].type == chunk_type::small_chunk) {
      const slot_count_type num_slots = slots(chunk_no);
      m_table[chunk_no].slot_occupancy.free(num_slots);
      m_table[chunk_no].init();

      if (chunk_no == m_last_used_chunk_no) {
        m_last_used_chunk_no = find_next_used_chunk_backward(chunk_no);
      }

    } else {
      m_table[chunk_no].init();
      chunk_no_type offset = 1;
      for (; chunk_no + offset < m_max_num_chunks &&
             m_table[chunk_no + offset].type == chunk_type::large_chunk_body;
           ++offset) {
        m_table[chunk_no + offset].init();
      }

      const chunk_no_type last_chunk_no = chunk_no + offset - 1;
      if (last_chunk_no == m_last_used_chunk_no) {
        m_last_used_chunk_no = find_next_used_chunk_backward(last_chunk_no);
      }
    }
  }

  /// \brief Finds an available slot in the chunk whose chunk number is
  /// 'chunk_no' and marks it as occupied. slot in the chunk. This function
  /// modifies only the specified chunk; thus, the global lock is not required.
  /// \param chunk_no Chunk number. \return Returns the marked slot number.
  slot_no_type find_and_mark_slot(const chunk_no_type chunk_no) {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = slots(chunk_no);
    assert(num_slots >= 1);

    assert(m_table[chunk_no].num_occupied_slots < num_slots);
    const auto empty_slot_no =
        m_table[chunk_no].slot_occupancy.find_and_set(num_slots);
    assert(empty_slot_no >= 0);
    ++m_table[chunk_no].num_occupied_slots;

    return empty_slot_no;
  }

  /// \brief Finds and marks multiple slots up to 'num_slots'. This function
  /// modifies only the specified chunk; thus, the global lock is not required.
  /// \param chunk_no Chunk number.
  /// \param num_slots Number of slots to find and mark.
  /// \param slots_buf Buffer to store found slots.
  /// \return Number of found slots.
  /// This number can be less than 'num_slots' if there are not enough available
  /// slots in the chunk.
  std::size_t find_and_mark_many_slots(const chunk_no_type chunk_no,
                                       const std::size_t num_slots,
                                       slot_no_type *const slots_buf) {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_holding_slots = slots(chunk_no);
    assert(num_holding_slots >= 1);

    assert(m_table[chunk_no].num_occupied_slots < num_holding_slots);
    const auto num_slots_to_find =
        std::min(num_slots,
                 static_cast<std::size_t>(
                     num_holding_slots - m_table[chunk_no].num_occupied_slots));
    m_table[chunk_no].slot_occupancy.find_and_set_many(
        num_holding_slots, num_slots_to_find, slots_buf);
    m_table[chunk_no].num_occupied_slots += num_slots_to_find;
    assert(m_table[chunk_no].num_occupied_slots <= num_holding_slots);

    return num_slots_to_find;
  }

  /// \brief
  /// \param chunk_no
  /// \param slot_no
  void unmark_slot(const chunk_no_type chunk_no, const slot_no_type slot_no) {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = slots(chunk_no);
    assert(num_slots >= 1);

    assert(m_table[chunk_no].num_occupied_slots > 0);
    m_table[chunk_no].slot_occupancy.reset(num_slots, slot_no);
    --m_table[chunk_no].num_occupied_slots;
  }

  /// \brief Returns if all slots in the chunk are marked.
  /// This function is valid only for a small chunk.
  /// \param chunk_no Chunk number. Must be less than size().
  /// \return Returns true if all slots in the chunk are marked.
  bool all_slots_marked(const chunk_no_type chunk_no) const {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots = slots(chunk_no);
    assert(num_slots >= 1);

    return (m_table[chunk_no].num_occupied_slots == num_slots);
  }

  /// \brief Returns if all slots in the chunk are unmarked.
  /// This function is valid only for a small chunk.
  /// Even if all slots are unmarked, the chunk still holds a slot table;
  /// thus, unused_chunk() return false.
  /// \param chunk_no Chunk number of a small chunk.
  /// \return Returns true if all slots in the chunk are unmarked.
  bool all_slots_unmarked(const chunk_no_type chunk_no) const {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);
    return (m_table[chunk_no].num_occupied_slots == 0);
  }

  /// \brief
  /// \param chunk_no
  /// \param slot_no
  /// \return
  bool marked_slot(const chunk_no_type chunk_no,
                   const slot_no_type slot_no) const {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const slot_count_type num_slots =
        calc_num_slots(bin_no_mngr::to_object_size(bin_no(chunk_no)));
    assert(slot_no < num_slots);
    return m_table[chunk_no].slot_occupancy.get(num_slots, slot_no);
  }

  /// \brief Returns the chunk directory size, which is the max chunk number +
  /// 1, not the number of used chunks.
  /// \return Returns the chunk directory size.
  /// \warning The returned value can be incorrect depending on the timing of
  /// another thread.
  std::size_t size() const { return m_last_used_chunk_no + 1; }

  /// \brief Returns true if a chunk is unused.
  /// 'unused' chunk means that the chunk is not used and does not hold any
  /// data or slot table.
  /// \param chunk_no Chunk number. Must be less than size().
  /// \return Returns true if the chunk is not used.
  bool unused_chunk(const chunk_no_type chunk_no) const {
    return (m_table[chunk_no].type == chunk_type::unused);
  }

  /// \brief
  /// \param chunk_no
  /// \return
  bin_no_type bin_no(const chunk_no_type chunk_no) const {
    return m_table[chunk_no].bin_no;
  }

  /// \brief
  /// \param chunk_no
  /// \return
  const slot_count_type slots(const chunk_no_type chunk_no) const {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);

    const auto bin_no = m_table[chunk_no].bin_no;
    return calc_num_slots(bin_no_mngr::to_object_size(bin_no));
  }

  /// \brief
  /// \param chunk_no
  /// \return
  slot_count_type occupied_slots(const chunk_no_type chunk_no) const {
    assert(m_table[chunk_no].type == chunk_type::small_chunk);
    return m_table[chunk_no].num_occupied_slots;
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

    for (chunk_no_type chunk_no = 0; chunk_no < size(); ++chunk_no) {
      if (unused_chunk(chunk_no)) {
        continue;
      }

      ofs << static_cast<uint64_t>(chunk_no) << " "
          << static_cast<uint64_t>(m_table[chunk_no].bin_no) << " "
          << static_cast<uint64_t>(m_table[chunk_no].type);
      if (!ofs) {
        std::stringstream ss;
        ss << "Something happened in the ofstream: " << path;
        logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
        return false;
      }

      if (m_table[chunk_no].type == chunk_type::small_chunk) {
        const slot_count_type num_slots = slots(chunk_no);
        ofs << " "
            << static_cast<uint64_t>(m_table[chunk_no].num_occupied_slots)
            << " " << m_table[chunk_no].slot_occupancy.serialize(num_slots)
            << "\n";
        if (!ofs) {
          std::stringstream ss;
          ss << "Something happened in the ofstream: " << path;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }

      } else if (m_table[chunk_no].type == chunk_type::large_chunk_head ||
                 m_table[chunk_no].type == chunk_type::large_chunk_body) {
        ofs << "\n";
        if (!ofs) {
          std::stringstream ss;
          ss << "Something happened in the ofstream: " << path;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }

      } else {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "Unexpected chunk status");
        return false;
      }
    }

    ofs.close();

    return true;
  }

  /// \brief
  /// \param path
  /// \return
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
    uint64_t buf3;
    while (ifs >> buf1 >> buf2 >> buf3) {
      const auto chunk_no = static_cast<chunk_no_type>(buf1);
      const auto bin_no = static_cast<bin_no_type>(buf2);
      m_table[chunk_no].bin_no = bin_no;

      using status_underlying_type = std::underlying_type_t<chunk_type>;
      const auto type = static_cast<status_underlying_type>(buf3);
      if (type ==
          static_cast<status_underlying_type>(chunk_type::small_chunk)) {
        m_table[chunk_no].type = chunk_type::small_chunk;
      } else if (type == static_cast<status_underlying_type>(
                             chunk_type::large_chunk_head)) {
        m_table[chunk_no].type = chunk_type::large_chunk_head;
      } else if (type == static_cast<status_underlying_type>(
                             chunk_type::large_chunk_body)) {
        m_table[chunk_no].type = chunk_type::large_chunk_body;
      } else {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "Invalid chunk type");
        return false;
      }

      if (m_table[chunk_no].type == chunk_type::small_chunk) {
        const slot_count_type num_slots =
            calc_num_slots(bin_no_mngr::to_object_size(bin_no));
        if (!(ifs >> buf1)) {
          std::stringstream ss;
          ss << "Cannot read a file: " << path;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }
        if (num_slots < buf1) {
          std::stringstream ss;
          ss << "Invalid num_occupied_slots: " << std::to_string(buf1);
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }
        m_table[chunk_no].num_occupied_slots = buf1;

        std::string bitset_buf;
        std::getline(ifs, bitset_buf);
        if (bitset_buf.empty() || bitset_buf[0] != ' ') {
          std::stringstream ss;
          ss << "Invalid input for slot_occupancy: " << bitset_buf;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }
        bitset_buf.erase(0, 1);

        if (!m_table[chunk_no].slot_occupancy.allocate(num_slots)) {
          logger::out(logger::level::error, __FILE__, __LINE__,
                      "Failed to allocate slot occupancy data");
          return false;
        }

        if (!m_table[chunk_no].slot_occupancy.deserialize(num_slots,
                                                          bitset_buf)) {
          std::stringstream ss;
          ss << "Invalid input for slot_occupancy: " << bitset_buf;
          logger::out(logger::level::error, __FILE__, __LINE__,
                      ss.str().c_str());
          return false;
        }
      }

      m_last_used_chunk_no = std::max((ssize_t)chunk_no, m_last_used_chunk_no);
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

  auto get_all_marked_slots() const {
    std::vector<std::tuple<chunk_no_type, bin_no_type, slot_no_type>> buf;

    for (chunk_no_type chunk_no = 0; chunk_no < size(); ++chunk_no) {
      if (m_table[chunk_no].type != chunk_type::small_chunk) {
        continue;
      }

      const slot_count_type num_slots = slots(chunk_no);
      for (slot_no_type i = 0; i < num_slots; ++i) {
        if (m_table[chunk_no].slot_occupancy.get(num_slots, i)) {
          buf.push_back(std::make_tuple(chunk_no, m_table[chunk_no].bin_no, i));
        }
      }
    }

    return buf;
  }

  std::size_t num_used_large_chunks() const {
    std::size_t count = 0;
    for (chunk_no_type chunk_no = 0; chunk_no < size(); ++chunk_no) {
      if (m_table[chunk_no].type == chunk_type::large_chunk_head ||
          m_table[chunk_no].type == chunk_type::large_chunk_body) {
        ++count;
      }
    }

    return count;
  }

 private:
  // -------------------- //
  // Private types and static values
  // -------------------- //

  // -------------------- //
  // Private methods
  // -------------------- //
  constexpr slot_count_type calc_num_slots(
      const std::size_t object_size) const {
    assert(k_chunk_size >= object_size);
    return k_chunk_size / object_size;
  }

  /// \brief Allocates memory for 'm_max_num_chunks' chunks.
  /// This function assumes that 'm_max_num_chunks' is set.
  /// Allocates 'uncommitted pages' so that not to waste physical memory until
  /// the pages are touched. Accordingly, this function does not initialize an
  /// allocate data.
  bool priv_allocate() {
    assert(!m_table);

    // Assume that mmap + MAP_ANONYMOUS returns 'uncommitted pages'.
    // An uncommitted page will be zero-initialized when it is touched first
    // time; however, this class does not rely on that. The table entries will
    // be initialized just before they are used.
    m_table = static_cast<entry_type *>(mdtl::map_anonymous_write_mode(
        nullptr, m_max_num_chunks * sizeof(entry_type)));

    if (!m_table) {
      logger::perror(logger::level::error, __FILE__, __LINE__,
                     "Cannot allocate chunk table");
      return false;
    }

    m_last_used_chunk_no = -1;
    return true;
  }

  void priv_destroy() noexcept {
    for (chunk_no_type chunk_no = 0; chunk_no < size(); ++chunk_no) {
      try {
        erase(chunk_no);
      } catch (...) {
        logger::perror(logger::level::error, __FILE__, __LINE__,
                       "An exception was thrown");
      }
    }
    mdtl::os_munmap(m_table, m_max_num_chunks * sizeof(entry_type));
    m_table = nullptr;
    m_last_used_chunk_no = -1;
  }

  /// \brief
  /// \param bin_no
  /// \param num_slots
  /// \return
  chunk_no_type priv_insert_small_chunk(const bin_no_type bin_no) {
    const slot_count_type num_slots =
        calc_num_slots(bin_no_mngr::to_object_size(bin_no));
    assert(num_slots > 1);
    if (num_slots > multilayer_bitset_type::max_size()) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Too many slots are requested.");
      return m_max_num_chunks;
    }

    for (chunk_no_type chunk_no = 0; chunk_no < m_max_num_chunks; ++chunk_no) {
      if (chunk_no > m_last_used_chunk_no || unused_chunk(chunk_no)) {
        m_table[chunk_no].init();  // init just in case

        m_table[chunk_no].bin_no = bin_no;
        m_table[chunk_no].type = chunk_type::small_chunk;
        m_table[chunk_no].num_occupied_slots = 0;
        if (!m_table[chunk_no].slot_occupancy.allocate(num_slots)) {
          logger::out(logger::level::error, __FILE__, __LINE__,
                      "Failed to allocates slot occupancy data");
          return m_max_num_chunks;
        }

        m_last_used_chunk_no =
            std::max((ssize_t)chunk_no, m_last_used_chunk_no);

        return chunk_no;
      }
    }
    logger::out(logger::level::error, __FILE__, __LINE__,
                "No empty chunk for small allocation");
    return m_max_num_chunks;
  }

  /// \brief
  /// \param bin_no
  /// \return
  chunk_no_type priv_insert_large_chunk(const bin_no_type bin_no) {
    const std::size_t num_chunks =
        (bin_no_mngr::to_object_size(bin_no) + k_chunk_size - 1) / k_chunk_size;
    assert(num_chunks >= 1);

    chunk_no_type count_continuous_empty_chunks = 0;
    for (chunk_no_type chunk_no = 0; chunk_no < m_max_num_chunks; ++chunk_no) {
      if (chunk_no > m_last_used_chunk_no) {
        // Initialize (empty) it before just in case
        m_table[chunk_no].init();
      }

      if (chunk_no <= m_last_used_chunk_no && !unused_chunk(chunk_no)) {
        count_continuous_empty_chunks = 0;
        continue;
      }

      ++count_continuous_empty_chunks;
      if (count_continuous_empty_chunks == num_chunks) {
        const chunk_no_type top_chunk_no =
            chunk_no - (count_continuous_empty_chunks - 1);
        m_table[top_chunk_no].bin_no = bin_no;
        m_table[top_chunk_no].type = chunk_type::large_chunk_head;

        for (chunk_no_type offset = 1; offset < num_chunks; ++offset) {
          m_table[top_chunk_no + offset].bin_no = bin_no;  // just in case
          m_table[top_chunk_no + offset].type = chunk_type::large_chunk_body;
        }

        m_last_used_chunk_no =
            std::max((ssize_t)chunk_no, m_last_used_chunk_no);

        return top_chunk_no;
      }
    }

    logger::out(logger::level::error, __FILE__, __LINE__,
                "No available space for large allocation, which requires "
                "multiple contiguous chunks");
    return m_max_num_chunks;
  }

  ssize_t find_next_used_chunk_backward(
      const chunk_no_type start_chunk_no) const {
    assert(start_chunk_no < size());
    for (ssize_t chunk_no = start_chunk_no; chunk_no >= 0; --chunk_no) {
      if (!unused_chunk(chunk_no)) {
        return chunk_no;
      }
    }
    return -1;
  }

  // -------------------- //
  // Private fields
  // -------------------- //
  entry_type *m_table;
  // Use const here to avoid race condition risks
  const std::size_t m_max_num_chunks;
  ssize_t m_last_used_chunk_no;
};

}  // namespace kernel
}  // namespace metall

#endif  // METALL_DETAIL_CHUNK_DIRECTORY_HPP
