[![Build Status](https://travis-ci.com/LLNL/metall.svg?branch=develop)](https://travis-ci.com/LLNL/metall)
[![Documentation Status](https://readthedocs.org/projects/metall/badge/?version=latest)](https://metall.readthedocs.io/en/latest/?badge=latest)

Metall (memory allocator for persistent memory)
===============================================

* Provides rich memory allocation interfaces for C++ applications that
  use persistent memory devices to persistently store heap data on such
  devices.
* Creates files in persistent memory and maps them into virtual memory
  space so that users can access the mapped region just as normal memory
  regions allocated in DRAM.
* To provide persistent memory allocation, Metall employs concepts and
  APIs developed by
  [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).
* Supports multi-threa
* Also provides a space-efficient snapshot/versioning leveraging reflink
  copy mechanism in filesystem. In case reflink is not supported, Metall
  automatically falls back to regular copy.


# Getting Started

## Install and Build

Metall consists of only header files and requires some header files in
Boost C++ Libraries.

Metall is available at:
[https://github.com/LLNL/metall](https://github.com/LLNL/metall).

All core files exist under
[metall/include/metall/](https://github.com/LLNL/metall/tree/develop/include/metall).

To build your program with Metall, all you have to do is just setting
include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,

```bash
g++ -std=c++17 -lstdc++fs your_program.cpp -I/path/to/metall/include -I/path/to/boost/include
```

Note GCC requires linking stdc++fs to use the Filesystem library in
C++17.


## Install Using Spack

Metall package is also available on [Spack](https://spack.io/).

To install Metall using Spack, type ```spack install metall```.

As Metall requires Boost C++ Libraries, Spack also installs a proper
(latest) version of Boost C++ Libraries automatically, if needed.

As Spack's load command configures environmental values properly, one
can avoid specifying include paths to build a program with Metall.
For instance:

```bash
spack load metall
g++ -std=c++17 -lstdc++fs your_program.cpp
```


## Required to Build Metall

- GCC 8.1 or more.
- Boost C++ Libraries 1.64 or more (build is not required; needs only
  their header files).

### Unofficial Support For Clang
Clang can be used instead of GCC to build Metall.
However, we haven't tested it intensively.
Also, Boost C++ Libraries 1.69 or more may be required
if one wants to build Metall with Clang + CUDA.


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
