// The macOS 26 SDK uses the C11 keyword _Static_assert at file scope in
// headers that are also compiled as C++ (mach/port.h, mach/message.h).
// clang accepts _Static_assert in C++ as an extension, gcc does not, so gcc
// fails on any C++ file that includes the mach headers.
// This mapping makes the SDK headers compile with gcc. The build system
// force-includes this file (-include) when it detects the problem, see
// cmake/check_gcc_macos_sdk.cmake.
#if defined(__GNUC__) && !defined(__clang__) && defined(__cplusplus)
#define _Static_assert static_assert
#endif
