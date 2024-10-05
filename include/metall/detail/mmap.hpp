// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_MMAP_HPP
#define METALL_DETAIL_UTILITY_MMAP_HPP

#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/falloc.h>  // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <string>
#include <cstdio>
#include <cerrno>
#include <sstream>

#include <metall/detail/memory.hpp>
#include <metall/detail/file.hpp>
#include <metall/detail/utilities.hpp>
#include <metall/logger.hpp>

namespace metall::mtlldetail {

/// \brief Maps a file checking errors
/// \param addr Same as mmap(2)
/// \param length Same as mmap(2)
/// \param protection Same as mmap(2)
/// \param flags Same as mmap(2)
/// \param fd Same as mmap(2)
/// \param offset  Same as mmap(2)
/// \return On success, returns a pointer to the mapped area.
/// On error, nullptr is returned.
inline void *os_mmap(void *const addr, const size_t length,
                     const int protection, const int flags, const int fd,
                     const off_t offset) {
  const ssize_t page_size = get_page_size();
  if (page_size == -1) {
    return nullptr;
  }

  if ((ptrdiff_t)addr % page_size != 0) {
    std::stringstream ss;
    ss << "address (" << addr << ") is not page aligned ("
       << ::sysconf(_SC_PAGE_SIZE) << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return nullptr;
  }

  if (offset % page_size != 0) {
    std::stringstream ss;
    ss << "offset (" << offset << ") is not a multiple of the page size ("
       << ::sysconf(_SC_PAGE_SIZE) << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return nullptr;
  }

  // ----- Map the file ----- //
  void *mapped_addr = ::mmap(addr, length, protection, flags, fd, offset);
  if (mapped_addr == MAP_FAILED) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "mmap");
    return nullptr;
  }

  if ((ptrdiff_t)mapped_addr % page_size != 0) {
    std::stringstream ss;
    ss << "mapped address (" << mapped_addr << ") is not page aligned ("
       << ::sysconf(_SC_PAGE_SIZE) << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return nullptr;
  }

  return mapped_addr;
}

/// \brief Map an anonymous region
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping \param length The length of the map
/// \param additional_flags Additional map flags
/// \return The starting address for the map. Returns nullptr on error.
inline void *map_anonymous_write_mode(void *const addr, const size_t length,
                                      const int additional_flags = 0) {
  return os_mmap(addr, length, PROT_READ | PROT_WRITE,
                 MAP_ANONYMOUS | MAP_PRIVATE | additional_flags, -1, 0);
}

/// \brief Map a file with read mode
/// \param file_name The name of file to be mapped
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping \param length The length of the map
/// \param offset The offset in the file
/// \param additional_flags Additional map flags
/// \return A pair of the file descriptor of the file and the starting address
/// for the map
inline std::pair<int, void *> map_file_read_mode(
    const fs::path &file_name, void *const addr, const size_t length,
    const off_t offset, const int additional_flags = 0) {
  // ----- Open the file ----- //
  const int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return std::make_pair(-1, nullptr);
  }

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ,
                              MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief Map a file with write mode.
/// \param fd  The file descriptor to map.
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping. \param length The length of the
/// map. \param offset The offset in the file. \return The starting address for
/// the map.
inline void *map_file_write_mode(const int fd, void *const addr,
                                 const size_t length, const off_t offset,
                                 const int additional_flags = 0) {
  if (fd == -1) return nullptr;

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ | PROT_WRITE,
                              MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    return nullptr;
  }

  return mapped_addr;
}

/// \brief Map a file with write mode
/// \param file_name The name of file to be mapped
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping \param length The length of the map
/// \param offset The offset in the file
/// \return A pair of the file descriptor of the file and the starting address
/// for the map
inline std::pair<int, void *> map_file_write_mode(
    const fs::path &file_name, void *const addr, const size_t length,
    const off_t offset, const int additional_flags = 0) {
  // ----- Open the file ----- //
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return std::make_pair(-1, nullptr);
  }

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ | PROT_WRITE,
                              MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief Map a file with write mode and MAP_PRIVATE.
/// \param fd  The file descriptor to map.
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping. \param length The length of the
/// map. \param offset The offset in the file. \return The starting address for
/// the map.
inline void *map_file_write_private_mode(const int fd, void *const addr,
                                         const size_t length,
                                         const off_t offset,
                                         const int additional_flags = 0) {
  if (fd == -1) return nullptr;

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    return nullptr;
  }

  return mapped_addr;
}

/// \brief Map a file with write mode and MAP_PRIVATE
/// \param file_name The name of file to be mapped
/// \param addr Normally nullptr; if this is not nullptr the kernel takes it as
/// a hint about where to place the mapping \param length The length of the map
/// \param offset The offset in the file
/// \return A pair of the file descriptor of the file and the starting address
/// for the map
inline std::pair<int, void *> map_file_write_private_mode(
    const fs::path &file_name, void *const addr, const size_t length,
    const off_t offset, const int additional_flags = 0) {
  // ----- Open the file ----- //
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    return std::make_pair(-1, nullptr);
  }

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ | PROT_WRITE,
                              MAP_PRIVATE | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

inline bool os_msync(void *const addr, const size_t length, const bool sync,
                     const int additional_flags = 0) {
  if (::msync(addr, length, (sync ? MS_SYNC : MS_ASYNC) | additional_flags) !=
      0) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "msync");
    return false;
  }
  return true;
}

inline bool os_munmap(void *const addr, const size_t length) {
  if (::munmap(addr, length) == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "munmap");
    return false;
  }
  return true;
}

inline bool munmap(void *const addr, const size_t length,
                   const bool call_msync) {
  if (call_msync) return os_msync(addr, length, true);
  return os_munmap(addr, length);
}

inline bool munmap(const int fd, void *const addr, const size_t length,
                   const bool call_msync) {
  bool ret = true;
  ret &= os_close(fd);
  ret &= munmap(addr, length, call_msync);
  return ret;
}

inline bool map_with_prot_none(void *const addr, const size_t length) {
  auto flag = MAP_PRIVATE | MAP_ANONYMOUS;
  if (addr != nullptr) flag |= MAP_FIXED;
  return (os_mmap(addr, length, PROT_NONE, flag, -1, 0) == addr);
}

inline bool os_mprotect(void *const addr, const size_t length, const int prot) {
  if (::mprotect(addr, length, prot) == -1) {
    logger::perror(logger::level::error, __FILE__, __LINE__, "mprotect");
    return false;
  }
  return true;
}

inline bool mprotect_read_only(void *const addr, const size_t length) {
  return os_mprotect(addr, length, PROT_READ);
}

inline bool mprotect_read_write(void *const addr, const size_t length) {
  return os_mprotect(addr, length, PROT_READ | PROT_WRITE);
}

// Does not log message because there are many situation where madvise fails
inline bool os_madvise(void *const addr, const size_t length, const int advice,
                       const std::size_t loop_safe_guard = 4) {
  int ret = -1;
  std::size_t loop_count = 0;
  do {
    ret = ::madvise(addr, length, advice);
    ++loop_count;
  } while (ret == -1 && errno == EAGAIN && loop_count < loop_safe_guard);

  return (ret == 0);
}

// NOTE: the MADV_FREE operation can be applied only to private anonymous pages.
inline bool uncommit_private_anonymous_pages(void *const addr,
                                             const size_t length) {
#ifdef MADV_FREE
  if (!os_madvise(addr, length, MADV_FREE)) {
    logger::perror(logger::level::verbose, __FILE__, __LINE__,
                   "madvise MADV_FREE");
    return false;
  }
#else
#ifdef METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
#warning "MADV_FREE is not defined. Metall uses MADV_DONTNEED instead."
#endif
  if (!os_madvise(addr, length, MADV_DONTNEED)) {
    logger::perror(logger::level::verbose, __FILE__, __LINE__,
                   "madvise MADV_DONTNEED");
    return false;
  }
#endif
  return true;
}

inline bool uncommit_private_nonanonymous_pages(void *const addr,
                                                const size_t length) {
  if (!os_madvise(addr, length, MADV_DONTNEED)) {
    logger::perror(logger::level::verbose, __FILE__, __LINE__,
                   "madvise MADV_DONTNEED");
    return false;
  }

  return true;
}

inline bool uncommit_shared_pages(void *const addr, const size_t length) {
  if (!os_madvise(addr, length, MADV_DONTNEED)) {
    logger::perror(logger::level::verbose, __FILE__, __LINE__,
                   "madvise MADV_DONTNEED");
    return false;
  }
  return true;
}

inline bool uncommit_shared_pages_and_free_file_space(
    [[maybe_unused]] void *const addr, [[maybe_unused]] const size_t length) {
#ifdef MADV_REMOVE
  if (!os_madvise(addr, length, MADV_REMOVE)) {
    logger::perror(logger::level::verbose, __FILE__, __LINE__,
                   "madvise MADV_REMOVE");
    return uncommit_shared_pages(addr, length);
  }
  return true;
#else
  return false;
#endif
}

/// \brief Reserve a VM region
/// \param length Length of the region to reserve
/// \return The address of the reserved region
inline void *reserve_vm_region(const size_t length) {
  /// MEMO: MAP_SHARED doesn't work at least when try to reserve a large size??
  void *mapped_addr =
      os_mmap(nullptr, length, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return mapped_addr;
}

/// \brief Reserve an aligned VM region
/// \param alignment Specifies the alignment. Must be a multiple of the system
/// page size \param length Length of the region to reserve \return The address
/// of the reserved region
inline void *reserve_aligned_vm_region(const size_t alignment,
                                       const size_t length) {
  const ssize_t page_size = get_page_size();
  if (page_size == -1) {
    return nullptr;
  }

  if (alignment % page_size != 0) {
    std::stringstream ss;
    ss << "alignment (" << alignment << ") is not a multiple of the page size ("
       << ::sysconf(_SC_PAGE_SIZE) << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return nullptr;
  }

  if (length % alignment != 0) {
    std::stringstream ss;
    ss << "length (" << length << ") is not a multiple of alignment ("
       << alignment << ")";
    logger::out(logger::level::error, __FILE__, __LINE__, ss.str().c_str());
    return nullptr;
  }

  void *const map_addr = os_mmap(nullptr, length + alignment, PROT_NONE,
                                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  void *const aligned_map_addr = reinterpret_cast<void *>(
      round_up(reinterpret_cast<size_t>(map_addr), alignment));

  // Trim the head
  const size_t surplus_head_length =
      reinterpret_cast<size_t>(aligned_map_addr) -
      reinterpret_cast<size_t>(map_addr);
  assert(surplus_head_length % page_size == 0);
  // assert(alignment <= surplus_head_length);
  if (surplus_head_length > 0 && !os_munmap(map_addr, surplus_head_length)) {
    return nullptr;
  }

  // Trim the tail
  const size_t surplus_tail_length = alignment - surplus_head_length;
  assert(surplus_tail_length % page_size == 0);
  if (surplus_tail_length > 0 &&
      !os_munmap(reinterpret_cast<char *>(aligned_map_addr) + length,
                 surplus_tail_length)) {
    return nullptr;
  }

  // The final check, just in case
  assert(reinterpret_cast<uint64_t>(aligned_map_addr) % alignment == 0);

  return aligned_map_addr;
}

class pagemap_reader {
 public:
  static constexpr uint64_t error_value = static_cast<uint64_t>(-1);

  pagemap_reader() : m_fd(-1) {
    m_fd = ::open("/proc/self/pagemap", O_RDONLY);
    if (m_fd < 0) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "Cannot open /proc/self/pagemap\n");
      logger::perror(logger::level::error, __FILE__, __LINE__, "open");
    }
  }

  ~pagemap_reader() { os_close(m_fd); }

  // Bits 0-54  page frame number (PFN) if present
  // Bits 0-4   swap type if swapped
  // Bits 5-54  swap offset if swapped
  // Bit  55    pte is soft-dirty (see Documentation/vm/soft-dirty.txt)
  // Bit  56    page exclusively mapped (since 4.2)
  // Bits 57-60 zero
  // Bit  61    page is file-page or shared-anon (since 3.5)
  // Bit  62    page swapped
  // Bit  63    page present
  uint64_t at(const uint64_t page_no) {
    if (m_fd < 0) {
      return error_value;
    }

    uint64_t buf;
    if (::pread(m_fd, &buf, sizeof(buf), page_no * sizeof(uint64_t)) == -1) {
      logger::perror(logger::level::error, __FILE__, __LINE__, "pread");
      return error_value;
    }

    if (buf &
        0x1E00000000000000ULL) {  // Sanity check; 57-60 bits are must be 0.
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "57-60 bits of the pagemap are not 0\n");
      return error_value;
    }

    return buf;
  }

 private:
  int m_fd;
};

}  // namespace metall::mtlldetail
#endif  // METALL_DETAIL_UTILITY_MMAP_HPP