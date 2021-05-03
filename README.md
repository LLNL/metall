[![metall-ci-test](https://github.com/LLNL/metall/actions/workflows/github-actions-test.yml/badge.svg)](https://github.com/LLNL/metall/actions/workflows/github-actions-test.yml)
[![Documentation Status](https://readthedocs.org/projects/metall/badge/?version=latest)](https://metall.readthedocs.io/en/latest/?badge=latest)

Metall: A Persistent Memory Allocator For Data-Centric Analytics
===============================================

* Provides rich memory allocation interfaces for C++ applications that
  use persistent memory devices to persistently store heap data on such
  devices.
* Creates files in persistent memory and maps them into virtual memory
  space so that users can access the mapped region just as normal memory
  regions allocated in DRAM.
* Actual persistent memory hardware could be any non-volatile memory (NVM) with file system support.
* To provide persistent memory allocation, Metall employs concepts and
  APIs developed by
  [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).
* Supports multi-thread
* Also provides a space-efficient snapshot/versioning, leveraging reflink
  copy mechanism in filesystem. In case reflink is not supported, Metall
  automatically falls back to regular copy.
* See details: [Metall overview slides](docs/publications/metall_overview.pdf).
  

# Getting Started

Metall consists of only header files and requires some header files in Boost C++ Libraries.

All core files exist under
[metall/include/metall/](https://github.com/LLNL/metall/tree/master/include/metall).

## Required

- GCC 8.1 or more (8.3 or more is recommended due to early implementation of the Filesystem library).

## Build

Boost C++ Libraries 1.64 or more is required (build is not required; needs only
their header files).

To build your program with Metall, all you have to do is just setting
include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,

```bash
# Download Boost (Boost C++ Libraries 1.64 or more is required)
# One can skip this step if Boost is already available.
wget https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz
tar xvf boost_1_75_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_75_0

git clone https://github.com/LLNL/metall
export METALL_INCLUDE=$PWD/metall/include

g++ -std=c++17 your_program.cpp -lstdc++fs -I${BOOST_ROOT} -I${METALL_INCLUDE}
```

### Unofficial Support For Clang
Clang can be used instead of GCC to build Metall.
However, we haven't tested it intensively.
Also, Boost C++ Libraries 1.69 or more may be required
if one wants to build Metall with Clang + CUDA.

## Metall with Spack

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


# Build Examples

Metall repository contains some example programs under [example directory](./example).
One can use CMake to build the examples.
For more details, see a page
[here](https://metall.readthedocs.io/en/latest/advanced_build/example_test_bench/).


# Documentation

[Full documentation](https://metall.readthedocs.io/) is available.


## Generate API document using Doxygen

A Doxygen configuration file is [here](docs/Doxyfile.in).

To generate API document:

```bash
cd metall
mkdir build_doc
cd build_doc
doxygen ../docs/Doxyfile.in
```


# Publication

## Metall: A Persistent Memory Allocator Enabling Graph Processing

[Paper PDF](https://www.osti.gov/servlets/purl/1576900)

[IEEE Xplore](https://ieeexplore.ieee.org/document/8945094)


# About

## Authors

* Keita Iwabuchi (kiwabuchi at llnl dot gov)
* Roger A Pearce (rpearce at llnl dot gov)
* Maya B Gokhale (gokhale2 at llnl dot gov).


## License

Metall is distributed under the terms of both the MIT license and the
Apache License (Version 2.0). Users may choose either license, at their
option.

All new contributions must be made under both the MIT and Apache-2.0
licenses.

See [LICENSE-MIT](LICENSE-MIT), [LICENSE-APACHE](LICENSE-APACHE),
[NOTICE](NOTICE), and [COPYRIGHT](COPYRIGHT) for details.

SPDX-License-Identifier: (Apache-2.0 OR MIT)


## Release

LLNL-CODE-768617
