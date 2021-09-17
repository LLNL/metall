
This page describes how to find Metall package from your CMake file.

## List of files in this directory

* [CMakeLists.txt](CMakeLists.txt)
  * An example CMake file that finds Metall 
* [c_example.c](c_example.c)
  * An example C file that depends on Metall C API
* [cpp_example.cpp](cpp_example.cpp)
  * An example CPP file that depends on Metall CXX API
 
# Build

Here is how to build the files in this directory.

### (pre-step) Install Metall
We assume that Metall is already installed.

To install Metall at `"/path/to/install"`, for example:
```bash
cd metall
mkdir build
cd build

cmake ../ -DDBOOST_ROOT=/path/to/boost -DCMAKE_INSTALL_PREFIX="/path/to/install" \
-DBUILD_C=ON # (option) use this option to build the Metall C API library

make && make install
```


### Build

Here is how to use the CMake file in this directory.

```bash
mkdir build
cd build

export CMAKE_PREFIX_PATH="/path/to/install"
cmake ../ -DDBOOST_ROOT=/path/to/boost
make
```


## Build With Spack

Here is how to build the files in this directory using Spack.

```bash
# Install Metall using Spack
# Boost is also install
spack install metall
spack load metall

mkdir build
cd build

cmake ../
make
```