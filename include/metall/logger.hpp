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
    silent = 10, /// Silent logger message — never show logger message
    critical = 5, /// Critical logger message — abort the execution unless disabled
    error = 4,  /// Error logger message
    warning = 3, /// Warning logger message
    info = 2, /// Info logger message
    debug = 1, /// Debug logger message
    verbose = 0, /// Verbose (lowest priority) logger message
  };

  /// \brief Set the minimum logger level to show message
  static void set_log_level(const level lvl) {
    log_message_out_level = lvl;
  }

  /// \brief If true is specified, enable an abort at a critical logger message
  static void abort_on_critical_error(const bool enable) {
    abort_on_critical = enable;
  }

  /// \brief Log a message to std::cerr if the specified logger level is equal to or higher than the pre-set logger level.
  static void out(const level lvl, const std::string &file_name, const int line_no, const std::string &message) {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    std::cerr << file_name << " at line " << line_no << " --- " << message << std::endl;

    if (lvl == level::critical && abort_on_critical) {
      std::abort();
    }
  }

  /// \brief Log a message about errno if the specified logger level is equal to or higher than the pre-set logger level.
  static void perror(const level lvl, const std::string &file_name, const int line_no, const std::string &message) {
    if (log_message_out_level == level::silent || lvl == level::silent || lvl < log_message_out_level)
      return;

    std::cerr << file_name << " at line " << line_no << " --- ";
    std::perror(message.c_str());
    // std::out << "errno is " << errno << std::endl;

    if (lvl == level::critical && abort_on_critical) {
      std::abort();
    }
  }

 private:
  static level log_message_out_level;
  static bool abort_on_critical;
};

logger::level logger::log_message_out_level = logger::level::error;
bool logger::abort_on_critical = true;

} // namespace metall

#endif //METALL_LOGGER_HPP
