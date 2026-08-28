# Build API Documentation, Examples, Tests, and Utilities

Metall's repository contains example, test, benchmark, verification, and
utility programs. This page describes how to configure and build them with
CMake.

```bash
git clone https://github.com/LLNL/metall
cd metall
cmake -S . -B build -DBUILD_EXAMPLE=ON
cmake --build build

# Optional: run tests if configured with -DBUILD_TEST=ON
ctest --test-dir build --output-on-failure

# Optional: install headers and package files
cmake --install build

# Optional: build API documentation if configured with -DBUILD_DOC=ON
cmake --build build --target build_doc
```

## Required

- CMake 3.14 or newer.
- A C++17 compiler.
- GCC 8.1 or newer is the primary tested compiler for building the repository.

## Boost C++ Libraries

Metall depends on Boost C++ Libraries 1.80 or newer.
For regular builds, Metall's CMake configuration can automatically fetch a
compatible Boost release when one is not already available.

To use an existing Boost checkout or a custom Boost archive, set exactly one of
the following options:

- `BOOST_INCLUDE_ROOT`: Legacy option. Use it to point at a directory that
    contains Boost headers. The given path is forwarded to the build targets'
    include path configuration.
- `BOOST_SOURCE_DIR`: Path to an unpacked Boost source tree that already exists
    on disk. Use this when you already have a Boost release that supports CMake.
- `BOOST_FETCH_URL`: URL or file path for a Boost source archive to fetch.
    Example:
    [boost-1.88.0-cmake.tar.gz](https://github.com/boostorg/boost/releases/download/boost-1.88.0/boost-1.88.0-cmake.tar.gz).
    The archive must contain a Boost release that supports CMake.

## Additional CMake Options

In addition to standard CMake variables, Metall defines several project
options. To list cached variables after configuration, run:

```bash
cmake -LAH -S . -B build
```

Here are some major options that you may want to use.

- `JUST_INSTALL_METALL_HEADER`: Install only Metall headers and package
    configuration files. Default: `OFF`.
- `BUILD_DOC`: Build the API documentation using Doxygen. You can also run
    Doxygen directly with `docs/Doxyfile.in`. Default: `OFF`.
- `BUILD_UTILITY`: Build utility programs under `src/`. Default: `OFF`.
- `BUILD_EXAMPLE`: Build examples under `example/`. Default: `OFF`.
- `BUILD_BENCH`: Build benchmarks under `bench/`. Default: `OFF`.
- `BUILD_TEST`: Build tests under `test/`. Google Test is downloaded
    automatically unless `SKIP_DOWNLOAD_GTEST=ON`. Default: `OFF`.
- `BUILD_VERIFICATION`: Build verification programs under `verification/`.
    Default: `OFF`.
- `BUILD_C`: Build the C interface library and related examples. Default:
    `OFF`.
- `RUN_LARGE_SCALE_TEST`: Enable large-scale test coverage where supported by
    the test suite. Default: `OFF`.

## Build 'test' Directory without Internet Access (experimental mode)

Step 1: On a machine with internet access, run CMake with
`ONLY_DOWNLOAD_GTEST=ON` to download Google Test into the build tree.

Step 2: Move that build directory to the offline machine, clear the cache if
needed, then reconfigure with `BUILD_TEST=ON` and `SKIP_DOWNLOAD_GTEST=ON`.

For example:

```bash
# On a machine with internet access
cd metall
cmake -S . -B build -DBUILD_TEST=ON -DONLY_DOWNLOAD_GTEST=ON

# On a machine without internet access
cd metall
rm -f build/CMakeCache.txt
cmake -S . -B build -DBUILD_TEST=ON -DSKIP_DOWNLOAD_GTEST=ON
```

- `ONLY_DOWNLOAD_GTEST`: Experimental option that downloads Google Test
    without building the rest of the test targets. Default: `OFF`. If
    `BUILD_TEST` is `OFF`, this option has no effect.
- `SKIP_DOWNLOAD_GTEST`: Experimental option that skips downloading Google
    Test. Default: `OFF`. If `BUILD_TEST` is `OFF`, this option has no effect.
