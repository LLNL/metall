// Copyright 2023 Lawrence Livermore National Security, LLC and other Metall
// Project Developers. See the top-level COPYRIGHT file for details.
//
// SPDX-License-Identifier: (Apache-2.0 OR MIT)

#include <metall/metall.hpp>

/// \brief converts the given level into string literal
const char *log_lvl_to_string(metall_log_level lvl) {
  switch (lvl) {
    case metall_critical: return "CRIT";
    case metall_error: return "ERROR";
    case metall_warning: return "WARN";
    case metall_info: return "INFO";
    case metall_debug: return "DEBUG";
    case metall_verbose: return "VERBOSE";
    default: return "UNKNOWN";
  }
}

/// \brief custom logger implementation
/// Output looks different than default.
extern "C" void metall_log(metall_log_level lvl, const char *file, size_t line_no, const char *message) {
  std::cerr << log_lvl_to_string(lvl) << " metall{file=" << file
            << ", line=" << line_no << "}: " << message << std::endl;
}

int main() {
  // Do Metall operations
  metall::manager manager(metall::create_only, "/tmp/dir");

  return 0;
}
