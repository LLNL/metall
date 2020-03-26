# Build Example, Test, and Benchmark Programs
Metall's repository contains some example, test, and benchmark programs.

```bash
git clone https://github.com/LLNL/metall
cd metall
mkdir build
cd build
cmake .. -DBOOST_ROOT=/path/to/boost/root/
make
make test    # option; BUILD_TEST must be ON
```

## Required

 - CMake 3.10 or more.
 - GCC 8.0 or more.
 - Boost C++ Libraries 1.64 or more (build is not required; needs only their header files).


## Additional CMake Options

In addition to the standard CMake options, we have two additional options:

* BUILD_BENCH
    * Builds subdirectory bench/
    * ON or OFF (default is ON).
    
* BUILD_TEST
    * Builds subdirectory test/
    * ON or OFF (default is OFF).
    * Google Test is automatically downloaded and built if BUILD_TEST is ON and SKIP_DOWNLOAD_GTEST is OFF.

* RUN_LARGE_SCALE_TEST
    * Runs large scale tests which could use ~ 100GB of storage space in /dev/shm or /tmp..
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option is ignored.


## Build 'test' Directory without Internet Access (experimental mode)

Step 1) Run CMake with ONLY_DOWNLOAD_GTEST=ON on a machine that has an internet access.

Step 2) Run CMake with BUILD_TEST=ON and SKIP_DOWNLOAD_GTEST=ON on a machine that does not have an internet access


For example,
```bash
# On a machine with the internet
cd metall
mkdir build
cd build
cmake ../ -DONLY_DOWNLOAD_GTEST=on # Use CMake to just download Google Test
# On a machine that does not have an internet access
cd metall/build
cmake ../ -DBUILD_TEST=on -DSKIP_DOWNLOAD_GTEST=on # Add other options you want to use
```

* ONLY_DOWNLOAD_GTEST
    * Experimental option
    * Only downloading Google Test (see more details below).
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option does not do anything.

* SKIP_DOWNLOAD_GTEST
    * Experimental option
    * Skips downloading Google Test (see more details below).
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option does not do anything.
