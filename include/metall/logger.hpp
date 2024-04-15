// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_LOGGER_HPP
#define METALL_LOGGER_HPP

#include <cstring>
#include <optional>
#include <sstream>
#include <metall/logger_interface.h>

namespace metall {

class logger {
 public:
  /// \brief Log message level
  enum struct level {
    /// \brief Critical logger message — with default logger implementation abort the execution unless disabled
    critical = metall_critical,
    /// \brief Error logger message
    error = metall_error,
    /// \brief Warning logger message
    warning = metall_warning,
    /// \brief Info logger message
    info = metall_info,
    /// \brief Debug logger message
    debug = metall_debug,
    /// \brief Verbose (lowest priority) logger message
    verbose = metall_verbose,
  };

  /// \brief Log a message
  static void out(const level lvl, const char* const file_name,
                  const int line_no, const char* const message) noexcept {
    metall_log(static_cast<metall_log_level>(lvl), file_name, line_no, message);
  }

  /// \brief Log a message about errno
  static void perror(const level lvl, const char* const file_name,
                     const int line_no, const char* const message) noexcept {
    std::stringstream ss;
    ss << message << ": " << strerror(errno);

    auto const m = ss.str();
    metall_log(static_cast<metall_log_level>(lvl), file_name, line_no, m.c_str());
  }

  logger() = delete;
  ~logger() = delete;
  logger(const logger&) = delete;
  logger(logger&&) = delete;
  logger& operator=(const logger&) = delete;
  logger& operator=(logger&&) = delete;

#ifndef METALL_LOGGER_EXTERN_C
  /// \brief determines the minimum level of messages that should be loggged
  struct level_filter {
  private:
    std::optional<level> inner;

    explicit level_filter(std::optional<level> inner) noexcept : inner{inner} {
    }

  public:
    /// \brief Only log critical messages
    static const level_filter critical;
    /// \brief Only log error and critical messages
    static const level_filter error;
    /// \brief Only log warning, error and critical messages
    static const level_filter warning;
    /// \brief Only log info, warning, error and critical messages
    static const level_filter info;
    /// \brief Only log debug, info, warning, error and critical messages
    static const level_filter debug;
    /// \brief Log all messages
    static const level_filter verbose;
    /// \brief Don't log any messages
    static const level_filter silent;

    /// \brief returns true if the logger should log a message of level lvl with this level_filter
    bool should_log(level lvl) const noexcept {
      return inner.has_value() && lvl >= *inner;
    }
  };

  /// \return the current minimum logger level
  static level_filter log_level() noexcept {
    return log_message_out_level;
  }

  /// \brief Set the minimum logger level to show message
  static void set_log_level(const level_filter lvl) noexcept {
    log_message_out_level = lvl;
  }

  /// \return if the program should abort when a critical message is logged
  static bool abort_on_critical_error() noexcept {
    return abort_on_critical;
  }

  /// \brief If true is specified, enable an abort at a critical logger message
  static void abort_on_critical_error(const bool enable) noexcept {
    abort_on_critical = enable;
  }

private:
  static level_filter log_message_out_level;
  static bool abort_on_critical;
#endif // METALL_LOGGER_EXTERN_C
};

#ifndef METALL_LOGGER_EXTERN_C
inline const logger::level_filter logger::level_filter::critical{logger::level::critical};
inline const logger::level_filter logger::level_filter::error{logger::level::error};
inline const logger::level_filter logger::level_filter::warning{logger::level::warning};
inline const logger::level_filter logger::level_filter::info{logger::level::info};
inline const logger::level_filter logger::level_filter::debug{logger::level::debug};
inline const logger::level_filter logger::level_filter::verbose{logger::level::verbose};
inline const logger::level_filter logger::level_filter::silent{std::nullopt};

inline logger::level_filter logger::log_message_out_level = logger::level_filter::error;
inline bool logger::abort_on_critical = true;
#endif // METALL_LOGGER_EXTERN_C

}  // namespace metall


#ifndef METALL_LOGGER_EXTERN_C
#include <iostream>

extern "C" inline void metall_log(metall_log_level lvl_, const char *file_name, size_t line_no, const char *message) {
  auto const lvl = static_cast<metall::logger::level>(lvl_);

  if (!metall::logger::log_level().should_log(lvl)) {
    return;
  }

  try {
    std::cerr << file_name << " at line " << line_no << " --- " << message
              << std::endl;
  } catch (...) {
  }

  if (lvl == metall::logger::level::critical && metall::logger::abort_on_critical_error()) {
    std::abort();
  }
}
#endif // METALL_LOGGER_EXTERN_C

#endif  // METALL_LOGGER_HPP
