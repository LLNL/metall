// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_LOGGER_HPP
#define METALL_LOGGER_HPP

#include <iostream>
#include <string>
#include <cassert>
#include <cstdio>

namespace metall {

class logger {
 public:

  /// \brief Log message level
  enum struct level {
    /// \brief Silent logger message — never show logger message
    silent = 10,
    /// \brief Critical logger message — abort the execution unless disabled
    critical = 5,
    /// \brief Error logger message
    error = 4,
    /// \brief Warning logger message
    warning = 3,
    /// \brief Info logger message
    info = 2,
    /// \brief Debug logger message
    debug = 1,
    /// \brief Verbose (lowest priority) logger message
    verbose = 0,
  };

  /// \brief Set the minimum logger level to show message
  static void set_log_level(const level lvl) noexcept {
    log_message_out_level = lvl;
  }

  /// \brief If true is specified, enable an abort at a critical logger message
  static void abort_on_critical_error(const bool enable) noexcept {
    abort_on_critical = enable;
  }

  /// \brief Log a message to std::cerr if the specified logger level is equal to or higher than the pre-set logger level.
  static void out(const level lvl, const char* const file_name, const int line_no, const char* const message) noexcept {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    try {
      std::cerr << file_name << " at line " << line_no << " --- " << message << std::endl;
    } catch (...) {}

    if (lvl == level::critical && abort_on_critical) {
      std::abort();
    }
  }

  /// \brief Log a message about errno if the specified logger level is equal to or higher than the pre-set logger level.
  static void perror(const level lvl, const char* const file_name, const int line_no, const char* const message) noexcept {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    try {
      std::cerr << file_name << " at line " << line_no << " --- ";
      std::perror(message);
    } catch (...) {}

    // std::out << "errno is " << errno << std::endl;

    if (lvl == level::critical && abort_on_critical) {
      std::abort();
    }
  }

  logger() = delete;
  ~logger() = delete;
  logger(const logger&) = delete;
  logger(logger&&) = delete;
  logger& operator=(const logger&) = delete;
  logger& operator=(logger&&) = delete;

 private:
  static level log_message_out_level;
  static bool abort_on_critical;
};

logger::level logger::log_message_out_level = logger::level::error;
bool logger::abort_on_critical = false;

} // namespace metall

#endif //METALL_LOGGER_HPP
