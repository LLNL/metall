// Copyright 2019 Lawrence Livermore National Security, LLC and other Metall Project Developers.
// See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <iostream>
#include <metall_utility/logger.hpp>

using namespace metall::utility;

void log() {
  logger::log(log_level::silent, "silent log");
  logger::log(log_level::critical, "critical log");
  logger::log(log_level::error, "error log");
  logger::log(log_level::warning, "warning log");
  logger::log(log_level::info, "info log");
  logger::log(log_level::debug, "debug log");
  logger::log(log_level::verbose, "verbose log");
}

int main() {
  logger::enable_abort(false);

  std::cerr << "--- Log level : unset ---" << std::endl;
  log();

  std::cerr << "\n--- Log level : silent ---" << std::endl;
  logger::set_log_level(log_level::silent);
  log();

  std::cerr << "\n--- Log level : critical ---" << std::endl;
  logger::set_log_level(log_level::critical);
  log();

  std::cerr << "\n--- Log level : error ---" << std::endl;
  logger::set_log_level(log_level::error);
  log();

  std::cerr << "\n--- Log level : warning ---" << std::endl;
  logger::set_log_level(log_level::warning);
  log();

  std::cerr << "\n--- Log level : info ---" << std::endl;
  logger::set_log_level(log_level::info);
  log();

  std::cerr << "\n--- Log level : debug ---" << std::endl;
  logger::set_log_level(log_level::debug);
  log();

  std::cerr << "\n--- Log level : verbose ---" << std::endl;
  logger::set_log_level(log_level::verbose);
  log();

  return 0;
}