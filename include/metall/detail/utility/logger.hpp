// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#ifndef METALL_DETAIL_UTILITY_LOGGER_HPP
#define METALL_DETAIL_UTILITY_LOGGER_HPP

#include <fstream>
#include <string>
#include <iostream>
#include <unordered_map>

namespace metall {
namespace detail {
namespace utility {

class logger_file {
 public:
  enum struct log_level {
    critical = 50,
    error = 40,
    warning = 30,
    info = 20,
    debug = 10,
    not_set = 0,
  };

  explicit logger_file(const std::string &file_prefix)
      : m_file_prefix(file_prefix),
        m_file_stream_table(),
        m_out_log_level(log_level::not_set) {}

  logger_file(const logger_file &) = delete;
  logger_file(logger_file &&) = delete;
  logger_file &operator=(const logger_file &) = delete;
  logger_file &operator=(logger_file &&) = delete;

  void set_out_log_level(const log_level level) {
    m_out_log_level = level;
  }

  void out(const std::string &log_file_name, const std::string message) {
    out(log_level::not_set, log_file_name, message);
  }

  void out(const log_level level, const std::string &log_file_name, const std::string message) {
    if (level < m_out_log_level) return;

    if (m_file_stream_table.count(log_file_name) == 0) {
      const bool is_succeeded = open_new_fstream(log_file_name);
      if (!is_succeeded) return;
    }

    std::ofstream &ofs = m_file_stream_table.at(log_file_name);
    if (!ofs.good()) {
      std::cerr << "Something happened to the ofstream of " << log_file_name << std::endl;
      return;
    }
    ofs << message << std::endl;
  }

 private:
  bool open_new_fstream(const std::string &log_file_name) {
    std::ofstream ofs(m_file_prefix + log_file_name, std::ofstream::out | std::ofstream::trunc);
    if (!ofs.is_open()) {
      std::cerr << "Can not open file : " << m_file_prefix + log_file_name << std::endl;
      return false;
    }

    m_file_stream_table[log_file_name] = std::move(ofs);
    return true;
  }

  std::string m_file_prefix;
  std::unordered_map<std::string, std::ofstream> m_file_stream_table;
  log_level m_out_log_level{log_level::not_set};
};

template <typename logger_type>
class logger_singleton {
 public:
  using log_level = typename logger_type::log_level;

  logger_singleton() = delete;
  logger_singleton(const logger_singleton &) = delete;
  logger_singleton(logger_singleton &&) = delete;
  logger_singleton &operator=(const logger_singleton &) = delete;
  logger_singleton &operator=(logger_singleton &&) = delete;

  /// TODO: change API of this function
  /// because this function must be called one time at the beginning
  static void set_log_file_prefix(const std::string &log_file_prefix) {
    if (m_has_log_file_prefix_set) {
      std::cerr << "The prefix of log files has been already set: " << m_has_log_file_prefix_set << std::endl;
      return;
    }
    m_has_log_file_prefix_set = true;
    instance(log_file_prefix);
  }

  static void set_out_log_level(const log_level level) {
    instance().set_out_log_level(level);
  }

  static void out(const std::string &log_file_name, const std::string &message) {
    if (m_has_log_file_prefix_set) {
      instance().out(log_file_name, message);
    }
  }

  static void out(const log_level level, const std::string &log_file_name, const std::string &message) {
    if (m_has_log_file_prefix_set) {
      instance().out(level, log_file_name, message);
    }
  }

 private:
  static bool m_has_log_file_prefix_set;

  static logger_type &instance(const std::string log_file_prefix = "") {
    static logger_type logger_instance(log_file_prefix);
    return logger_instance;
  }
};
template <typename T>
bool logger_singleton<T>::m_has_log_file_prefix_set = false;

using logger = logger_singleton<logger_file>;

} // namespace utility
} // namespace detail
} // namespace metall
#endif //METALL_DETAIL_UTILITY_LOGGER_HPP
