// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>

int main() {
  // Set the log level to , for example, verbose.
  // The log level can be changed at any time.
  metall::logger::set_log_level(metall::logger::level_filter::verbose);

  // Enable the program to abort when a critical log message is displayed.
  metall::logger::abort_on_critical_error(true);

  // Do Metall operations
  metall::manager manager(metall::create_only, "/tmp/dir");

  return 0;
}