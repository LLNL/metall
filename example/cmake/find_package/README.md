# Finding Metall from a CMake Project

This example shows how to **find and link an already installed** Metall
package from a CMake project, using CMake's [`find_package`](https://cmake.org/cmake/help/latest/command/find_package.html).

To have CMake download, build, and link Metall instead, see the
[FetchContent example](../FetchContent).

The [CMakeLists.txt](CMakeLists.txt) in this directory:
- Calls `find_package(Metall REQUIRED)`. `MetallConfig.cmake` resolves (and,
  if needed, fetches) a suitable Boost on its own, so no separate Boost setup
  is required here.
- Builds `cpp_example`, which links `Metall::Metall` (the C++ API).
- Builds `c_example`, which links `Metall::metall_c` (the C API), if the
  library was installed (i.e. Metall was built with `-DBUILD_C=ON`).

## 1. Install Metall

Metall must be installed before this example can find it.

### Option A: Install manually

```bash
cd metall
mkdir build
cd build

# Add -DBUILD_C=ON to also build/install the Metall C API library.
cmake ../ -DCMAKE_INSTALL_PREFIX="/path/to/install"
make && make install
```

### Option B: Install with Spack

```bash
# Boost is installed automatically as a dependency.
spack install metall
```

Note: the Metall C API is not supported when installed via Spack.

## 2. Build this example

```bash
mkdir build
cd build

# Point CMake at the Metall install from step 1.
export CMAKE_PREFIX_PATH="/path/to/install"
# Or, if installed with Spack, this exports CMAKE_PREFIX_PATH (and BOOST_ROOT):
# spack load metall

cmake ../
make
```

This produces `cpp_example` and, if `Metall::metall_c` was installed,
`c_example` in the build directory. Run either directly, e.g. `./cpp_example`.

