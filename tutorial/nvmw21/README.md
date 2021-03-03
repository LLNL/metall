# Run Tutorial Programs

## Required
- GCC 8.1 or more

## Build and Run
```bash
# Download Boost (Boost C++ Libraries 1.64 or more is required)
# One can skip this step if Boost is already available.
wget https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz
tar xvf boost_1_75_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_75_0

git clone https://github.com/LLNL/metall
cd metall/tutorial/nvmw21
g++ -std=c++17 [tutorial_program.cpp] -lstdc++fs -I../../include -I${BOOST_ROOT}

# All tutorial programs does not take command-line options
./a.out
```


## Build using Clang (or Apple clang)

Clang (or Apple clang) could be used instead of GCC to build Metall.
However, please note that we haven't tested it intensively.


### On macOS >= 10.15 or Linux

```bash
# Remove "-lstdc++fs" option
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT}
```


### On macOS < 10.15

The C++17 <filesystem> library is not available on macOS < 10.15.
One has to stop using C++17 <filesystem> library in Metall.
If METALL_NOT_USE_CXX17_FILESYSTEM_LIB macro is defined, Metall uses its own file system operation implementation.

```bash
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT} -DMETALL_NOT_USE_CXX17_FILESYSTEM_LIB
```