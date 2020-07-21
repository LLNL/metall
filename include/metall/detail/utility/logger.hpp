// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_LOGGER_HPP
#define METALL_DETAIL_UTILITY_LOGGER_HPP

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>

namespace metall {
namespace detail {
namespace utility {

class log {
 public:

  /// \brief Log message level
  enum struct level {
    silent = 10, /// Silent log message — never show log message
    critical = 5, /// Critical log message — abort the execution unless disabled
    error = 4,  /// Error log message
    warning = 3, /// Warning log message
    info = 2, /// Info log message
    debug = 1, /// Debug log message
    verbose = 0, /// Verbose (lowest priority) log message
  };

  /// \brief Set the minimum log level to show message
  static void set_log_level(const level lvl) {
    log_message_out_level = lvl;
  }

  /// \brief If true is specified, enable an abort at a critical log message
  static void enable_abort(const bool enable) {
    abort_at_critical = enable;
  }

  /// \brief Log a message to std::cerr if the specified log level is equal to or higher than the pre-set log level.
  static void out(const level lvl, const std::string &file_name, const int line_no, const std::string &message) {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    std::cerr << file_name << " at line " << line_no << " --- " << message << std::endl;

    if (lvl == level::critical && abort_at_critical) {
      std::abort();
    }
  }

  /// \brief Log a message about errno if the specified log level is equal to or higher than the pre-set log level.
  static void perror(const level lvl, const std::string &file_name, const int line_no, const std::string &message) {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    std::cerr << file_name << " at line " << line_no << " --- ";
    std::perror(message.c_str());
    // std::out << "errno is " << errno << std::endl;

    if (lvl == level::critical && abort_at_critical) {
      std::abort();
    }
  }

 private:
  static level log_message_out_level;
  static bool abort_at_critical;
};

log::level log::log_message_out_level = log::level::error;
bool log::abort_at_critical = true;

} // namespace utility
} // namespace detail
} // namespace metall

#endif //METALL_DETAIL_UTILITY_LOGGER_HPP
