// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/detail/utility/logger.hpp>

using namespace metall::detail::utility;

void log_cerr() {
  log::out(log::level::silent, __FILE__, __LINE__, "silent log");
  log::out(log::level::critical, __FILE__, __LINE__, "critical log");
  log::out(log::level::error, __FILE__, __LINE__, "error log");
  log::out(log::level::warning, __FILE__, __LINE__, "warning log");
  log::out(log::level::info, __FILE__, __LINE__, "info log");
  log::out(log::level::debug, __FILE__, __LINE__, "debug log");
  log::out(log::level::verbose, __FILE__, __LINE__, "verbose log");
}

void log_perror() {
  log::perror(log::level::silent, __FILE__, __LINE__, "silent log");
  log::perror(log::level::critical, __FILE__, __LINE__, "critical log");
  log::perror(log::level::error, __FILE__, __LINE__, "error log");
  log::perror(log::level::warning, __FILE__, __LINE__, "warning log");
  log::perror(log::level::info, __FILE__, __LINE__, "info log");
  log::perror(log::level::debug, __FILE__, __LINE__, "debug log");
  log::perror(log::level::verbose, __FILE__, __LINE__, "verbose log");
}

int main() {
  log::enable_abort(false);

  std::cerr << "--- Log level : unset ---" << std::endl;
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : silent ---" << std::endl;
  log::set_log_level(log::level::silent);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : critical ---" << std::endl;
  log::set_log_level(log::level::critical);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : error ---" << std::endl;
  log::set_log_level(log::level::error);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : warning ---" << std::endl;
  log::set_log_level(log::level::warning);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : info ---" << std::endl;
  log::set_log_level(log::level::info);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : debug ---" << std::endl;
  log::set_log_level(log::level::debug);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : verbose ---" << std::endl;
  log::set_log_level(log::level::verbose);
  log_cerr();
  log_perror();

  return 0;
}