# Downloading and Linking Metall with `FetchContent`

This example shows how to download, build, and link Metall directly from a
CMake project using CMake's [`FetchContent`](https://cmake.org/cmake/help/latest/module/FetchContent.html)
module — no separate install step required.

To link an *already installed* Metall package instead, see the
[find_package example](../find_package).

The [CMakeLists.txt](CMakeLists.txt) in this directory:
- Fetches the Metall source with `FetchContent` and adds it as a subdirectory.
- Metall's own build resolves (and, if needed, fetches) a suitable Boost, so
  no separate Boost setup is required here.
- Builds `cpp_example`, which links `Metall::Metall` (the C++ API).
- Optionally builds `c_example`, which links `Metall::metall_c` (the C API),
  when `BUILD_C` is enabled.

## Build

```bash
mkdir build
cd build

# BUILD_C=ON is required only if you also want to build the C API example.
cmake ../ -DBUILD_C=ON
make
```

This produces `cpp_example` and (with `BUILD_C=ON`) `c_example` in the build
directory. Run either directly, e.g. `./cpp_example`.

