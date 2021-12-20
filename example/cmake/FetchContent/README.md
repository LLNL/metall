
On this page, we describe how to download, install, and link Metall from a CMake project.

To just find/link an already installed Metall package from a CMake project, see another example [here](../find_package).

Here is an example CMake file that downloads, installs, and links Metall package [CMakeLists.txt](CMakeLists.txt).

## Build

Here is how to use the CMake file in this directory.

```bash
mkdir build
cd build

# BOOST_ROOT: Path to Boost C++ Libraries (option)
#  If Boost is not found in the local environment, the CMake file will download it automatically.
# BUILD_C: Required to use Metall C (not 'C++') API (option)
cmake ../ -DBOOST_ROOT=/path/to/boost -DBUILD_C=ON
make
```
