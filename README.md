[![CI Test](https://github.com/LLNL/metall/actions/workflows/ci-test.yml/badge.svg?branch=master)](https://github.com/LLNL/metall/actions/workflows/ci-test.yml)
[![Documentation Status](https://readthedocs.org/projects/metall/badge/?version=latest)](https://metall.readthedocs.io/en/latest/?badge=latest)
[![Deploy API Doc](https://github.com/LLNL/metall/actions/workflows/deploy-api-doc.yml/badge.svg?branch=master)](https://github.com/LLNL/metall/actions/workflows/deploy-api-doc.yml)

Metall: A Persistent Memory Allocator for Data-Centric Analytics
===============================================

- A memory allocator enables applications to transparently allocate data into a file system.

  - From applications, Metall looks like a normal heap allocator.

  - Applications can keep their data beyond single process life cycles and reattach the data in succeeding runs.

  - Leverages the memory-mapped file mechanism (i.e., [mmap(2)](https://man7.org/linux/man-pages/man2/mmap.2.html)) to _map_ application data in files to the main memory.

- Employs the API developed by [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).

  - Useful for allocating C++ data structures (including STL containers).

- Incorporates state-of-the-art allocation algorithms.

- Provides a space-efficient snapshot/versioning, leveraging the reflink copy mechanism in file systems.

- See details: [Metall overview slides](docs/publications/metall_101.pdf).

# Getting Started

Metall consists of only header files and requires some header files in Boost C++ Libraries.

All core files exist under
[metall/include/metall/](https://github.com/LLNL/metall/tree/master/include/metall).

## Required

- Boost C++ Libraries 1.80 or more.
  - Build is not required; needs only their header files.
- C++17 compiler
  - Tested with GCC 8.1 or more; however, 8.3 or more is recommended due to early implementation of the C++ Filesystem library.

## Build

To build your program with Metall, all you have to do is just setting include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,

```bash
# Download Boost (Boost C++ Libraries 1.80 or more is required)
# One can skip this step if Boost is already available.
wget https://boostorg.jfrog.io/artifactory/main/release/1.80.0/source/boost_1_80_0.tar.gz
tar xvf boost_1_80_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_80_0

git clone https://github.com/LLNL/metall
export METALL_INCLUDE=$PWD/metall/include

g++ -std=c++17 your_program.cpp -lstdc++fs -I${BOOST_ROOT} -I${METALL_INCLUDE}
```

### Unofficial Support For Clang
Clang can be used instead of GCC to build Metall.
However, we haven't tested it intensively.

## Package Manager Support

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

### Metall with Connan

Metall is also available on [Conan](https://conan.io/), thanks to the [DICE](https://github.com/dice-group) research group.
Conan Metall package information is [here](https://conan.io/center/recipes/metall).


## Use Metall from Another CMake Project

To download and/or link Metall package from a CMake project,
see example CMake files placed [here](./example/cmake).

# Build Example Programs

Metall repository contains some example programs under [example directory](./example).
One can use CMake to build the examples.
For more details, see a page
[here](https://metall.readthedocs.io/en/latest/advanced_build/cmake/).


# Documentations

- [Full documentation](https://metall.readthedocs.io/)
- [API documentation](https://software.llnl.gov/metall/api/)

## Generate API documentation using Doxygen

A Doxygen configuration file is [here](docs/Doxyfile.in).

To generate API document:

```bash
cd metall
mkdir build_doc
cd build_doc
doxygen ../docs/Doxyfile.in
```


# Publication

```
Keita Iwabuchi, Karim Youssef, Kaushik Velusamy, Maya Gokhale, Roger Pearce,
Metall: A persistent memory allocator for data-centric analytics,
Parallel Computing, 2022, 102905, ISSN 0167-8191, https://doi.org/10.1016/j.parco.2022.102905.
```

* [Parallel Computing](https://www.sciencedirect.com/science/article/abs/pii/S0167819122000114) (journal)

* [arXiv](https://arxiv.org/abs/2108.07223) (preprint)

# About

## Contact

- [GitHub Issues](https://github.com/LLNL/metall/issues) is open.
  
- Primary contact: [Keita Iwabuchi (LLNL)](https://github.com/KIwabuchi).

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
