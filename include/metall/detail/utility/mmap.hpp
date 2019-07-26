// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_MMAP_HPP
#define METALL_DETAIL_UTILITY_MMAP_HPP

#include <string>

#include <cstdio>
#include <cerrno>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifdef __linux__
#include <linux/falloc.h> // For FALLOC_FL_PUNCH_HOLE and FALLOC_FL_KEEP_SIZE
#endif

#include <metall/detail/utility/memory.hpp>

namespace metall {
namespace detail {
namespace utility {

/// \brief Maps a file checking errors
/// \param addr Same as mmap(2)
/// \param length Same as mmap(2)
/// \param protection Same as mmap(2)
/// \param flags Same as mmap(2)
/// \param fd Same as mmap(2)
/// \param offset  Same as mmap(2)
/// \return On success, returns a pointer to the mapped area.
/// On error, nullptr is returned.
inline void *os_mmap(void *const addr, const size_t length, const int protection, const int flags,
                     const int fd, const off_t offset) {
  const ssize_t page_size = get_page_size();
  if (page_size == -1) {
    return nullptr;
  }

  if ((ptrdiff_t)addr % page_size != 0) {
    std::cerr << "address (" << addr << ") is not page aligned ("
              << ::sysconf(_SC_PAGE_SIZE) << ")" << std::endl;
    return nullptr;
  }

  if (offset % page_size != 0) {
    std::cerr << "offset (" << offset << ") is not a multiple of the page size (" << ::sysconf(_SC_PAGE_SIZE) << ")"
              << std::endl;
    return nullptr;
  }

  // ----- Map the file ----- //
  void *mapped_addr = ::mmap(addr, length, protection, flags, fd, offset);
  if (mapped_addr == MAP_FAILED) {
    ::perror("mmap");
    std::cerr << "errno: " << errno << std::endl;
    return nullptr;
  }

  if ((ptrdiff_t)mapped_addr % page_size != 0) {
    std::cerr << "mapped address (" << mapped_addr << ") is not page aligned ("
              << ::sysconf(_SC_PAGE_SIZE) << ")" << std::endl;
    return nullptr;
  }

  return mapped_addr;
}

/// \brief Map an anonymous region
/// \param addr Normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length The lenght of the map
/// \param additional_flags Additional map flags
/// \return The starting address for the map. Returns nullptr on error.
inline void *map_anonymous_write_mode(void *const addr,
                                      const size_t length,
                                      const int additional_flags = 0) {
  return os_mmap(addr, length, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE | additional_flags, -1, 0);
}

/// \brief Map a file with read mode
/// \param file_name The name of file to be mapped
/// \param addr Normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length The lenght of the map
/// \param offset The offset in the file
/// \param additional_flags Additional map flags
/// \return A pair of the file descriptor of the file and the starting address for the map
inline std::pair<int, void *> map_file_read_mode(const std::string &file_name, void *const addr,
                                                 const size_t length, const off_t offset,
                                                 const int additional_flags = 0) {
  // ----- Open the file ----- //
  const int fd = ::open(file_name.c_str(), O_RDONLY);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return std::make_pair(-1, nullptr);
  }

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ, MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

/// \brief Map a file with write mode
/// \param file_name The name of file to be mapped
/// \param addr Normaly nullptr; if this is not nullptr the kernel takes it as a hint about where to place the mapping
/// \param length The lenght of the map
/// \param offset The offset in the file
/// \return A pair of the file descriptor of the file and the starting address for the map
inline std::pair<int, void *> map_file_write_mode(const std::string &file_name, void *const addr,
                                                  const size_t length, const off_t offset,
                                                  const int additional_flags = 0) {
  // ----- Open the file ----- //
  const int fd = ::open(file_name.c_str(), O_RDWR);
  if (fd == -1) {
    ::perror("open");
    std::cerr << "errno: " << errno << std::endl;
    return std::make_pair(-1, nullptr);
  }

  // ----- Map the file ----- //
  void *mapped_addr = os_mmap(addr, length, PROT_READ | PROT_WRITE, MAP_SHARED | additional_flags, fd, offset);
  if (mapped_addr == nullptr) {
    close(fd);
    return std::make_pair(-1, nullptr);
  }

  return std::make_pair(fd, mapped_addr);
}

inline bool os_msync(void *const addr, const size_t length, const bool sync) {
  if (::msync(addr, length, sync ? MS_SYNC : MS_ASYNC) != 0) {
    // ::perror("msync");
    // std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}

inline bool os_munmap(void *const addr, const size_t length) {
  if (::munmap(addr, length) != 0) {
    ::perror("munmap");
    std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}

inline bool munmap(void *const addr, const size_t length, const bool call_msync) {
  if (call_msync) return os_msync(addr, length, true);
  return os_munmap(addr, length);
}

inline bool munmap(const int fd, void *const addr, const size_t length, const bool call_msync) {
  ::close(fd);
  return munmap(addr, length, call_msync);
}

inline bool map_with_prot_none(void *const addr, const size_t length) {
  return (os_mmap(addr, length, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0) == addr);
}

// NOTE: the MADV_FREE operation can be applied only to private anonymous pages.
inline bool uncommit_private_pages(void *const addr, const size_t length) {
#ifdef MADV_FREE
  if (::madvise(addr, length, MADV_FREE) != 0) {
    // ::perror("madvise MADV_FREE");
    // std::cerr << "errno: " << errno << std::endl;
    return false;
  }
#else
  #warning "MADV_FREE is not defined. Metall uses MADV_DONTNEED instead."
  if (::madvise(addr, length, MADV_DONTNEED) != 0) {
    // ::perror("madvise MADV_DONTNEED");
    // std::cerr << "errno: " << errno << std::endl;
    return false;
  }
#endif
  return true;
}

inline bool uncommit_shared_pages(void *const addr, const size_t length) {
  if (::madvise(addr, length, MADV_DONTNEED) != 0) {
    // ::perror("madvise MADV_DONTNEED");
    // std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
}

inline bool uncommit_file_backed_pages([[maybe_unused]] void *const addr,
                                       [[maybe_unused]] const size_t length) {
#ifdef MADV_REMOVE
  if (::madvise(addr, length, MADV_REMOVE) != 0) {
    // ::perror("madvise MADV_REMOVE");
    // std::cerr << "errno: " << errno << std::endl;
    return false;
  }
  return true;
#else
#warning "MADV_REMOVE is not supported. Metall cannot free file space."
  return uncommit_shared_pages(addr, length);
#endif
}

/// \brief Reserve a vm address region
/// \param length Length of region
/// \return The address of the region
inline void *reserve_vm_region(const size_t length) {
  /// MEMO: MAP_SHARED doesn't work at least when try to reserve a large size??
  void *mapped_addr = os_mmap(nullptr, length, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  return mapped_addr;
}

class pagemap_reader {
 public:
  static constexpr uint64_t error_value = static_cast<uint64_t>(-1);

  pagemap_reader()
      : m_fd(-1) {
    m_fd = ::open("/proc/self/pagemap", O_RDONLY);
    if (m_fd < 0) {
      ::perror("open");
      std::cerr << "Cannot open /proc/self/pagemap" << std::endl;
    }
  }

  ~pagemap_reader() {
    ::close(m_fd);
  }

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
      // ::perror("pread");
      return error_value;
    }

    if (buf & 0x1E00000000000000ULL) { // Sanity check; 57-60 bits are must be 0.
      // std::cerr << "57-60 bits of the pagemap are not 0" << std::endl;
      return error_value;
    }

    return buf;
  }

 private:
  int m_fd;
};

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY_MMAP_HPP