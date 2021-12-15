# Find Metall from CMake Project

On this page, we describe how to **find/link an already installed** Metall package from a CMake project.

To have CMake download, install, and link Metall, see another example [here](../FetchContent).

Here is an example CMake file that finds an already installed Metall package [CMakeLists.txt](CMakeLists.txt).

## 0. (pre-step) Install Metall

Here is how to build example programs.

### 0-1. Install Metall Manually

To install Metall at `"/path/to/install"`, for example:
```bash
cd metall
mkdir build
cd build

cmake ../ -DCMAKE_INSTALL_PREFIX="/path/to/install"
# (option) add the following options to build the Metall C API library
-DBOOST_ROOT=/path/to/boost -DBUILD_C=ON 

make && make install
```


### 0-2. Install Metall using Spack

Alternatively, one can install Metall using Spack.

Please note that Metall C API is not supported with Spack. 

```bash
# Install Metall using Spack
# Boost is also install
spack install metall
```


## 1. Build

Here is how to use the CMake file in this directory.

```bash
mkdir build
cd build

export CMAKE_PREFIX_PATH="/path/to/install"
# Or (if one uses Spack)
spack load metall # Spack exports CMAKE_PREFIX_PATH (and also BOOST_ROOT).

cmake ../ \
-DBOOST_ROOT=/path/to/boost # Required if one wants to build programs that uses Metall C++ API.
# Or (if one uses Spack)
cmake ../

make
```
