
On this page, we describe how to download, install, and link Metall from a CMake project.

To just find/link an already installed Metall package from a CMake project, see another example [here](../find_package).

Here is an example CMake file that downloads, installs, and links Metall package [CMakeLists.txt](CMakeLists.txt).

## Build

Here is how to use the CMake file in this directory.

```bash
mkdir build
cd build

cmake ../ -DBOOST_ROOT=/path/to/boost
make
```
