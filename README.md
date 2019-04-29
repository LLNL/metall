Metall (Meta Allocator)
====================
* Provides simplified memory allocation interfaces for C++ applications that use persistent memory devices to persistently store heap data on such devices.
* Creates files in persistent devices and map them into virtual memory space so that users can access the mapped region just as normal memory regions allocated in DRAM.
* To provide persistent memory allocation, Metall employs concepts and APIs developed by [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).
* Metall supports multi-thread
* Example programs that use Metall are listed [here](#example).



# Getting Started



## Use Metall in Your Project

Metall consists of only header files and requires some header files in Boost C++ Libraries.
All core files exist under [metall/include/metall/](./include/metall).
All you have to do is just setting include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,
```bash
g++ -std=c++17 -lstdc++fs your_program.cpp -I/path/to/metall/include -I/path/to/boost/include
```
Note GCC requires linking stdc++fs to use the Filesystem library in C++17.


### Required

 - GCC (g++) 8.0 or more.
 - Boost C++ Libraries 1.60 or more (build is not required; needs only their header files).



## Limitations To Store Objects Persistently

To store objects persistently, there are some limitations as listed below.

* Raw pointers
    * When store pointers persistently, raw pointers have to be replaced with offset pointers because there is no guarantee that backing-files are mapped to the same virtual memory address every time.
    * An offset pointer holds an offset between the address pointing at and itself.
* References
    * References have to be removed due to the same reason as raw pointers.
* Virtual Function and Virtual Base Class
    * As the virtual table also uses raw pointers, virtual functions and virtual base classes are not allowed.
* STL Containers
    * Some of STL containers' implementations do not work with Metall ([see detail](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.stl_container_requirements)).
    * We recommend using [containers implemented in Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.containers)
     as they fully support persistent allocations.

Some example programs that use Metall are listed [here](#example).



## Build Example, Test, and Benchmark Programs
Metall's repository contains some example, test, and benchmark programs.

```bash
git clone [this repository]
cd metall
mkdir build
cd build
cmake .. -DBOOST_ROOT=/path/to/boost/root/
make
make test    # option; BUILD_TEST must be ON
```

### Required

 - cmake 3.8 or more.
 - GCC (g++) 8.0 or more.
 - Boost C++ Libraries 1.60 or more (build is not required; needs only their header files).


### Additional Cmake Options

In addition to the standard cmake options, we have two additional options:
* BUILD_BENCH
    * Builds subdirectory bench/
    * ON or OFF (default is ON).
* BUILD_TEST
    * Builds subdirectory test/
    * ON or OFF (default is OFF).
    * Google Test is automatically downloaded and built if BUILD_TEST is ON and SKIP_GTEST_DOWNLOAD is OFF.
* RUN_LARGE_SCALE_TEST
    * Runs large scale tests which could use ~ 100GB of storage space in /dev/shm or /tmp..
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option is ignored.
* SKIP_GTEST_DOWNLOAD
    * Experimental option
    * Skips downloading Google Test (see more details below).
    * ON or OFF (default is OFF).
    * If BUILD_TEST is OFF, this option does not do anything.


### Build 'test' Directory without Internet Access (experimental mode)

    Step 1) Run cmake with BUILD_TEST=ON on a machine that has an internet access.
    Step 2) Run cmake with BUILD_TEST=ON and SKIP_GTEST_DOWNLOAD=ON on a machine that does not have an internet access

For example,
```bash
# On a machine with the internet
cmake ../ -DBUILD_TEST=on # Use cmake to just download Google Test. You might also need specify BOOST_ROOT option
# On a machine without the internet
cmake ../ -DBUILD_TEST=on -DSKIP_GTEST_DOWNLOAD=on # also other options you want to use
```
Google Test is downloaded in the first step and built in the second step.


## Example

Example programs are located in [example/](example/)
* [simple.cpp](./example/simple.cpp)
    * A simple example of allocating a vector container.

* [vector_of_vectors.cpp](./example/vector_of_vectors.cpp)
    * An example of nested (multi-level) containers.

* [offset_pointer.cpp](./example/offset_pointer.cpp)
    * An example code using the offset pointer to store pointer persistently.

* [snapshot.cpp](./example/snapshot.cpp)
    * An example code that snapshots and copies the snapshot files to a new place.


# Authors

* Keita Iwabuchi (kiwabuchi at llnl dot gov)
* Roger A Pearce (rpearce at llnl dot gov)
* Maya B Gokhale (gokhale2 at llnl dot gov).



# License

Metall is distributed under the terms of both the MIT license and the Apache License (Version 2.0).
Users may choose either license, at their option.

All new contributions must be made under both the MIT and Apache-2.0 licenses.

See [LICENSE-MIT](LICENSE-MIT), [LICENSE-APACHE](LICENSE-APACHE), [NOTICE](NOTICE), and [COPYRIGHT](COPYRIGHT) for details.

SPDX-License-Identifier: (Apache-2.0 OR MIT)



# Release

LLNL-CODE-768617
