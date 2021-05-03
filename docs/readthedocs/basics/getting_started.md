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


## Build Example

Metall depends on Boost C++ Libraries 1.64 or more (build is not required; needs only
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
Also, Boost C++ Libraries 1.69 or more may be required
if one wants to build Metall with Clang + CUDA.

**On macOS >= 10.15 or Linux**

```bash
# Remove "-lstdc++fs" option
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT}
```

**On macOS < 10.15**

The C++17 <filesystem> library is not available on macOS < 10.15.
One has to stop using C++17 <filesystem> library in Metall.
If METALL_NOT_USE_CXX17_FILESYSTEM_LIB macro is defined, Metall uses its own file system operation implementation.

```bash
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT} -DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB
```
