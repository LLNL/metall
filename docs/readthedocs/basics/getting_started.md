# Getting Started

This page shows how to use Metall from your own program.

Metall's core library is header-only and depends on Boost headers.

All core files exist under
[metall/include/metall/](https://github.com/LLNL/metall/tree/master/include/metall).

## Required

- Linux or macOS: Metall is developed primarily for Linux. Some performance
    optimizations are disabled on macOS, and Metall does not support Microsoft
    Windows.
- GCC 8.1 or more: GCC 8.3 or newer is recommended because early Filesystem
    implementations had limitations.
- Boost C++ Libraries 1.80 or more: a Boost build is not required; only the
    headers are needed.

## Build Example

Metall depends on Boost C++ Libraries 1.80 or newer, but you only need the
headers.

To build a program with Metall, add the Boost and Metall include directories to
your compiler flags.

For example:

```bash
# Download Boost (Boost C++ Libraries 1.80 or more is required)
# Skip this step if Boost is already available on your system.
wget https://archives.boost.io/release/1.80.0/source/boost_1_80_0.tar.gz
tar xvf boost_1_80_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_80_0

git clone https://github.com/LLNL/metall
export METALL_INCLUDE=$PWD/metall/include

g++ -std=c++17 your_program.cpp -I${BOOST_ROOT} -I${METALL_INCLUDE}
```

If you build with GCC earlier than 9.1 and your program uses the Filesystem
library, you may also need to link `-lstdc++fs`.

### Metall with Spack

Metall package is also available on [Spack](https://spack.io/).

As Metall depends on Boost C++ Libraries,
Spack also installs a proper version of Boost C++ Libraries automatically, if needed.

```bash
# Install Metall and Boost C++ Libraries
spack install metall

# Sets environment variables: BOOST_ROOT and METALL_ROOT.
# Boost C++ Libraries and Metall are installed at the locations, respectively.
spack load metall

# Build a program that uses Metall
# Please note that one has to put 'include' at the end of BOOST_ROOT and METALL_ROOT
g++ -std=c++17 your_program.cpp -I${BOOST_ROOT}/include -I${METALL_ROOT}/include
```

## Build Using Clang or Apple clang

Clang (or Apple clang) could be used instead of GCC to build Metall.
However, we have not tested it as extensively as GCC.
To run on macOS, Metall requires macOS >= 10.15.

```bash
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT}
```
