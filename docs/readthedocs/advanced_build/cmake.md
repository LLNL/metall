# Build API document, Example, Test, and Utility Programs

Metall's repository contains example, test, benchmark, and utility programs. 
Here is how to build them using CMake.

```bash
git clone https://github.com/LLNL/metall
cd metall
mkdir build
cd build
cmake .. -DBOOST_ROOT=/path/to/boost/root/
make
make test  # option; BUILD_TEST must be ON when running cmake
make install # option; Use CMake CMAKE_INSTALL_PREFIX variable to configure install destinations.
cmake build_doc  # option; BUILD_DOC must be ON when running cmake
```

## Required

 - CMake 3.10 or more.
 - GCC 8.1 or more.
 - Boost C++ Libraries 1.64 or more (build is not required; needs only their header files).


## Additional CMake Options

In addition to the standard CMake options, Metall have additional options as follows:

* BUILD_DOC
    * Build API document using Doxygen
    * One can also build the document by using doxygen directly; see README.md in the repository of Metall.
    * ON or OFF (default is OFF)

* BUILD_UTILITY 
    * Build utility programs under src/
    * ON or OFF (default is OFF)

* BUILD_EXAMPLE
    * Build examples under example/
    * ON or OFF (default is OFF)

* BUILD_BENCH
    * Builds subdirectory bench/
    * ON or OFF (default is OFF).
    
* BUILD_TEST
    * Builds subdirectory test/
    * ON or OFF (default is OFF).
    * Google Test is automatically downloaded and built if BUILD_TEST is ON and SKIP_DOWNLOAD_GTEST is OFF.

* RUN_LARGE_SCALE_TEST
    * Runs large scale tests which could use ~ 100GB of storage space in /dev/shm or /tmp..
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option is ignored.

* BUILD_C
    * Build a library for C interface
    * ON or OFF (default is OFF).


## Build 'test' Directory without Internet Access (experimental mode)

Step 1) Run CMake with ONLY_DOWNLOAD_GTEST=ON on a machine that has an internet access.

Step 2) Run CMake with BUILD_TEST=ON and SKIP_DOWNLOAD_GTEST=ON on a machine that does not have an internet access


For example,
```bash
# On a machine with the internet
cd metall
mkdir build
cd build
cmake ../ -DBUILD_TEST=ON -DONLY_DOWNLOAD_GTEST=on # Use CMake to just download Google Test
# On a machine that does not have an internet access
cd metall/build
rm CMakeCache.txt
cmake ../ -DBUILD_TEST=on -DSKIP_DOWNLOAD_GTEST=on # Add other options you want to use
```

* ONLY_DOWNLOAD_GTEST
    * Experimental option
    * Only downloading Google Test (see more details below).
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option does nothing.

* SKIP_DOWNLOAD_GTEST
    * Experimental option
    * Skips downloading Google Test (see more details below).
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option does not do anything.
