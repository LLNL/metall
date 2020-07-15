// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_UTILITY_LOGGER_HPP
#define METALL_UTILITY_LOGGER_HPP

#include <iostream>
#include <string>

namespace metall {
namespace utility {

enum struct log_level {
  silent = 10, // Never show log message
  critical = 5, // Critical log message, abort the execution unless disabled
  error = 4,
  warning = 3,
  info = 2,
  debug = 1,
  verbose = 0
};

namespace detail {
class logger_cerr {
 public:
  logger_cerr() = default;
  logger_cerr(const logger_cerr &) = default;
  logger_cerr(logger_cerr &&) = default;
  logger_cerr &operator=(const logger_cerr &) = default;
  logger_cerr &operator=(logger_cerr &&) = default;

  /// \brief Set the minimum log level to print out message
  static void set_log_level(const log_level level) {
    m_log_message_out_level = level;
  }

  /// \brief If true is specified, enable an abort at a critical log  message
  static void enable_abort(const bool enable) {
    m_abort_at_critical = enable;
  }

  /// \brief Log a message without setting log level
  static void log(const std::string &message) {
    log(log_level::verbose, message);
  }

  /// \brief Log a message if the specified log level is equal to or higher than the level to show.
  static void log(const log_level level, const std::string &message) {
    if (m_log_message_out_level == log_level::silent || level == log_level::silent || level < m_log_message_out_level)
      return;
    std::cerr << message << std::endl;

    if (level == log_level::critical && m_abort_at_critical) {
      std::abort();
    }
  }

 private:
  static log_level m_log_message_out_level;
  static bool m_abort_at_critical;
};
log_level logger_cerr::m_log_message_out_level = log_level::silent;
bool logger_cerr::m_abort_at_critical = true;

} // namespace detail

using logger = detail::logger_cerr;

} // namespace utility
} // namespace metall
#endif //METALL_UTILITY_LOGGER_HPP
