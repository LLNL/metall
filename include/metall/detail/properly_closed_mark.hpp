#ifndef METALL_DETAIL_PROPERLYCLOSED_MARK_HPP
#define METALL_DETAIL_PROPERLYCLOSED_MARK_HPP

#include <metall/tags.hpp>
#include <metall/detail/file.hpp>

#include <cassert>
#include <filesystem>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>

namespace metall::mtlldetail {

/**
 * A marker file on the filesystem that determines if and how the datastore
 * is currently opened and if it was properly closed the last time it was opened.
 *
 * It allows multiple readers xor one writer.
 * It uses the existence of the file in combination with flock(2) to achieve this.
 */
struct properly_closed_mark {
private:
  // if mark_path is empty, this mark is in an invalid state
  // and should not create the mark on close
  std::filesystem::path m_mark_path{};
  int m_mark_fd = -1;
  bool m_read_only = false;

  void unlock() {
    if (m_mark_fd < 0) {
      return;
    }

    ::flock(m_mark_fd, LOCK_UN);
    ::close(m_mark_fd);
    m_mark_fd = -1;
  }

  bool open_mark() {
    const int fd = ::open(m_mark_path.c_str(), O_RDONLY);
    if (fd < 0) {
      return false;
    }

    m_mark_fd = fd;
    return true;
  }

  bool lock_exclusive() {
    if (!open_mark()) {
      return false;
    }

    const int ret = ::flock(m_mark_fd, LOCK_EX | LOCK_NB);
    return ret == 0;
  }

  bool lock_shared() {
    if (!open_mark()) {
      return false;
    }

    const int ret = ::flock(m_mark_fd, LOCK_SH | LOCK_NB);
    return ret == 0;
  }

  bool open_impl() {
    if (m_read_only) {
      return lock_shared();
    } else {
      if (!lock_exclusive()) {
        return false;
      }

      return remove_file(m_mark_path);
    }
  }

public:
  properly_closed_mark() noexcept = default;

  bool create(const std::filesystem::path &path) {
    m_mark_path = path;
    m_read_only = false;

    if (!remove_file(m_mark_path)) {
      // could not create, prevent mark creation on close
      m_mark_path.clear();
      return false;
    }

    return true;
  }

  bool open(const std::filesystem::path &path, const bool read_only) {
    m_mark_path = path;
    m_read_only = read_only;

    if (!open_impl()) {
      // could not open, prevent mark creation on close
      m_mark_path.clear();
      return false;
    }

    return true;
  }

  properly_closed_mark(const properly_closed_mark &other) = delete;
  properly_closed_mark &operator=(const properly_closed_mark &other) = delete;

  properly_closed_mark(properly_closed_mark &&other) noexcept
    : m_mark_path{std::move(other.m_mark_path)},
      m_mark_fd{std::exchange(other.m_mark_fd, -1)},
      m_read_only{other.m_read_only} {
  }

  properly_closed_mark &operator=(properly_closed_mark &&other) noexcept {
    assert(this != &other);
    std::swap(m_mark_path, other.m_mark_path);
    std::swap(m_mark_fd, other.m_mark_fd);
    std::swap(m_read_only, other.m_read_only);
    return *this;
  }

  ~properly_closed_mark() noexcept {
    close();
  }

  void close() {
    if (!m_read_only && !m_mark_path.empty()) {
      create_file(m_mark_path);
    }
    unlock();
  }

  [[nodiscard]] bool is_read_only() const noexcept {
    return m_read_only;
  }
};

} // namespace metall::mtlldetail

#endif //  METALL_DETAIL_PROPERLYCLOSED_MARK_HPP
