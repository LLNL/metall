# Getting Started

In this page, we describe how to use Metall with your program.

Metall consists of only header files and requires some header files in Boost C++ Libraries.

All core files exist under
[metall/include/metall/](https://github.com/LLNL/metall/tree/master/include/metall).

## Required

- Linux or macOS
    - Metall is developed for Linux
    - Some performance optimizations will be disabled on macOS
    - Metall does not work on Microsoft Windows


- GCC 8.1 or more
    - 8.3 or more is recommended due to early implementation of the Filesystem library

- Boost C++ Libraries 1.80 or more
    - Build is not required; needs only the header files.

## Build Example

Metall depends on Boost C++ Libraries 1.80 or more (build is not required; needs only
their header files).

To build your program with Metall, all you have to do is just setting
include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,

```bash
# Download Boost (Boost C++ Libraries 1.80 or more is required)
# One can skip this step if Boost is already available.
wget https://dl.bintray.com/boostorg/release/1.80.0/source/boost_1_80_0.tar.gz
tar xvf boost_1_80_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_80_0

git clone https://github.com/LLNL/metall
export METALL_INCLUDE=$PWD/metall/include

g++ -std=c++17 your_program.cpp -lstdc++fs -I${BOOST_ROOT} -I${METALL_INCLUDE}
```

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
g++ -std=c++17 your_program.cpp -lstdc++fs -I${BOOST_ROOT}/include -I${METALL_ROOT}/include
```

## Build Using Clang or Apple clang

Clang (or Apple clang) could be used instead of GCC to build Metall.
However, we haven't tested it intensively.
To run on macOS, Metall requires macOS >= 10.15.

```bash
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT}
```