// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall/logger.hpp>

using namespace metall;

void log_cerr() {
  logger::out(logger::level::critical, __FILE__, __LINE__, "critical logger");
  logger::out(logger::level::error, __FILE__, __LINE__, "error logger");
  logger::out(logger::level::warning, __FILE__, __LINE__, "warning logger");
  logger::out(logger::level::info, __FILE__, __LINE__, "info logger");
  logger::out(logger::level::debug, __FILE__, __LINE__, "debug logger");
  logger::out(logger::level::verbose, __FILE__, __LINE__, "verbose logger");
}

void log_perror() {
  logger::perror(logger::level::critical, __FILE__, __LINE__,
                 "critical logger");
  logger::perror(logger::level::error, __FILE__, __LINE__, "error logger");
  logger::perror(logger::level::warning, __FILE__, __LINE__, "warning logger");
  logger::perror(logger::level::info, __FILE__, __LINE__, "info logger");
  logger::perror(logger::level::debug, __FILE__, __LINE__, "debug logger");
  logger::perror(logger::level::verbose, __FILE__, __LINE__, "verbose logger");
}

int main() {
  logger::abort_on_critical_error(false);

  std::cerr << "--- Log level : unset ---" << std::endl;
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : silent ---" << std::endl;
  logger::set_log_level(logger::level_filter::silent);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : critical ---" << std::endl;
  logger::set_log_level(logger::level_filter::critical);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : error ---" << std::endl;
  logger::set_log_level(logger::level_filter::error);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : warning ---" << std::endl;
  logger::set_log_level(logger::level_filter::warning);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : info ---" << std::endl;
  logger::set_log_level(logger::level_filter::info);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : debug ---" << std::endl;
  logger::set_log_level(logger::level_filter::debug);
  log_cerr();
  log_perror();

  std::cerr << "\n--- Log level : verbose ---" << std::endl;
  logger::set_log_level(logger::level_filter::verbose);
  log_cerr();
  log_perror();

  return 0;
}