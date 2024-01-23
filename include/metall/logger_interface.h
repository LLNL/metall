#ifndef METALL_LOGGER_INTERFACE_H
#define METALL_LOGGER_INTERFACE_H

#ifdef __cplusplus
extern "C" {
#endif

/// \brief Log message level
typedef enum metall_log_level {
  /// \brief Critical logger message
  metall_critical = 5,
  /// \brief Error logger message
  metall_error = 4,
  /// \brief Warning logger message
  metall_warning = 3,
  /// \brief Info logger message
  metall_info = 2,
  /// \brief Debug logger message
  metall_debug = 1,
  /// \brief Verbose (lowest priority) logger message
  metall_verbose = 0,
} metall_log_level;


/// \brief Implementation of logging behaviour.
///
/// If METALL_LOGGER_EXTERN_C is defined this function does not have an implementation
/// and is intended to be implemented by consuming applications.
///
/// If it is not defined the default implementation is in metall/logger.hpp.
void metall_log(metall_log_level lvl, const char *file_name, size_t line_no, const char *message);

#ifdef __cplusplus
}
#endif

#endif // METALL_LOGGER_INTERFACE_H
