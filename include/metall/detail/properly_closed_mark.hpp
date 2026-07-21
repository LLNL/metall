#ifndef METALL_DETAIL_PROPERLYCLOSED_MARK_HPP
#define METALL_DETAIL_PROPERLYCLOSED_MARK_HPP

#include <metall/detail/file.hpp>
#include <metall/logger.hpp>

#include <cassert>
#include <filesystem>
#include <utility>

#include <fcntl.h>
#include <sys/file.h>
#include <unistd.h>

namespace metall::mtlldetail {

/**
 * Open-state tracking and crash detection for a datastore.
 *
 * Two files with separate roles:
 *
 * - A lockfile. It always exists while a datastore exists. It is never
 *   removed. flock(2) on it allows multiple readers or one writer. Because
 *   the file is persistent, creating a datastore can take the lock before it
 *   destroys existing data, and a failed open cannot be confused with a
 *   crashed datastore.
 *
 * - A mark file (the properly-closed mark). A read-write open removes it and
 *   makes the removal durable. A successful close recreates it durably. Its
 *   absence at open time means the datastore was not properly closed.
 *
 * The mark is only created by an explicit call to mark_properly_closed().
 * The destructor only releases the lock. The caller creates the mark after
 * it has verified that all datastore state reached disk.
 */
struct properly_closed_mark {
 private:
  int m_lock_fd = -1;
  // if m_mark_path is empty, this instance holds no open datastore
  std::filesystem::path m_mark_path{};
  bool m_read_only = false;

  static int open_or_create_lockfile(
      const std::filesystem::path &lockfile_path) {
    const int fd = ::open(lockfile_path.c_str(), O_RDONLY);
    if (fd >= 0) {
      return fd;
    }
    if (errno != ENOENT) {
      std::string s("open lockfile: " + lockfile_path.string());
      logger::perror(logger::level::error, __FILE__, __LINE__, s.c_str());
      return -1;
    }

    // The lockfile does not exist yet (datastore created by an older version).
    const int new_fd =
        ::open(lockfile_path.c_str(), O_CREAT | O_RDONLY, S_IRUSR | S_IWUSR);
    if (new_fd < 0) {
      std::string s("create lockfile: " + lockfile_path.string());
      logger::perror(logger::level::error, __FILE__, __LINE__, s.c_str());
      return -1;
    }
    // The lockfile must exist before the lock has any meaning for other
    // processes.
    fsync_directory(lockfile_path.parent_path());
    return new_fd;
  }

  bool lock(const std::filesystem::path &lockfile_path,
                 const bool shared) {
    assert(m_lock_fd == -1);

    const int fd = open_or_create_lockfile(lockfile_path);
    if (fd < 0) {
      return false;
    }

    if (::flock(fd, (shared ? LOCK_SH : LOCK_EX) | LOCK_NB) != 0) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "The datastore is already open (lockfile is locked)");
      ::close(fd);
      return false;
    }

    m_lock_fd = fd;
    return true;
  }

  void unlock() {
    if (m_lock_fd < 0) {
      return;
    }
    ::flock(m_lock_fd, LOCK_UN);
    ::close(m_lock_fd);
    m_lock_fd = -1;
  }

 public:
  properly_closed_mark() noexcept = default;

  properly_closed_mark(const properly_closed_mark &) = delete;
  properly_closed_mark &operator=(const properly_closed_mark &) = delete;

  properly_closed_mark(properly_closed_mark &&other) noexcept
      : m_lock_fd{std::exchange(other.m_lock_fd, -1)},
        m_mark_path{std::move(other.m_mark_path)},
        m_read_only{other.m_read_only} {
    other.m_mark_path.clear();
  }

  properly_closed_mark &operator=(properly_closed_mark &&other) noexcept {
    assert(this != &other);
    std::swap(m_lock_fd, other.m_lock_fd);
    std::swap(m_mark_path, other.m_mark_path);
    std::swap(m_read_only, other.m_read_only);
    return *this;
  }

  /// The destructor only releases the lock. It does not create the mark,
  /// because the state of the datastore is unknown here.
  ~properly_closed_mark() noexcept { release(); }

  /// \brief Takes the writer lock for a datastore that is being created.
  /// The mark is not touched. The caller destroys and recreates the datastore
  /// directory (which removes any old mark) while the lock is held.
  /// \return On success, returns true. On error (including a concurrently
  /// open datastore), returns false.
  bool create(const std::filesystem::path &lockfile_path,
              const std::filesystem::path &mark_path) {
    if (!lock(lockfile_path, false)) {
      return false;
    }
    m_mark_path = mark_path;
    m_read_only = false;
    return true;
  }

  /// \brief Takes the lock and checks the mark for an existing datastore.
  /// Writer: takes the exclusive lock, requires the mark, removes it and makes
  /// the removal durable before the caller modifies any data.
  /// Reader: takes the shared lock and requires the mark.
  /// \return On success, returns true. On error (already open, or not
  /// properly closed), returns false.
  bool open(const std::filesystem::path &lockfile_path,
            const std::filesystem::path &mark_path, const bool read_only) {
    if (!lock(lockfile_path, read_only)) {
      return false;
    }

    if (!file_exist(mark_path)) {
      logger::out(logger::level::error, __FILE__, __LINE__,
                  "The datastore was not closed properly and may be broken");
      unlock();
      return false;
    }

    if (!read_only) {
      if (!remove_file(mark_path) ||
          !fsync_directory(mark_path.parent_path())) {
        logger::out(logger::level::error, __FILE__, __LINE__,
                    "Failed to durably remove the properly-closed mark");
        unlock();
        return false;
      }
    }

    m_mark_path = mark_path;
    m_read_only = read_only;
    return true;
  }

  /// \brief Creates the mark durably.
  /// The caller invokes this only after all datastore state was synced to
  /// disk. The lock stays held until release().
  /// \return On success, returns true. On error, returns false.
  bool mark_properly_closed() {
    if (m_mark_path.empty() || m_read_only) {
      return false;
    }
    // create_file syncs the file and its parent directory.
    return create_file(m_mark_path);
  }

  /// \brief Releases the lock without touching the mark.
  void release() {
    unlock();
    m_mark_path.clear();
  }

  [[nodiscard]] bool is_read_only() const noexcept { return m_read_only; }
};

}  // namespace metall::mtlldetail

#endif  //  METALL_DETAIL_PROPERLYCLOSED_MARK_HPP
