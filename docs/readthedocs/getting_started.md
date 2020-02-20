# Getting Started



## Use Metall in Your Project

Metall consists of only header files and requires some header files in Boost C++ Libraries.
All core files exist under [metall/include/metall/](./include/metall).
All you have to do is just setting include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,
```bash
g++ -std=c++17 -lstdc++fs your_progr/Users/iwabuchi1/git_local/metall_folk/mkdocs.ymlam.cpp -I/path/to/metall/include -I/path/to/boost/include
```
Note GCC requires linking stdc++fs to use the Filesystem library in C++17.


### Required

 - GCC 8.0 or more.
 - Boost C++ Libraries 1.64 or more (build is not required; needs only their header files).



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


## Compile-time Options
There are some compile-time options as follows to configure the behavior of Metall:
* METALL_USE_SPACE_AWARE_BIN
	* If defined, Metall tries to fill memory from lower addresses.
	
* METALL_DISABLE_FREE_FILE_SPACE
	* If defined, Metall does not free file space

* METALL_FREE_SMALL_OBJECT_SIZE_HINT=*N*
	* Experimental option
	* If defined, Metall tries to free space when an object equal or larger than *N* bytes is deallocated.


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

 - CMake 3.10 or more.
 - GCC 8.0 or more.
 - Boost C++ Libraries 1.64 or more (build is not required; needs only their header files).


### Additional CMake Options

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


### Build 'test' Directory without Internet Access (experimental mode)

    Step 1) Run CMake with ONLY_DOWNLOAD_GTEST=ON on a machine that has an internet access.
    Step 2) Run CMake with BUILD_TEST=ON and SKIP_DOWNLOAD_GTEST=ON on a machine that does not have an internet access

For example,
```bash
# On a machine with the internet
cmake ../ -DONLY_DOWNLOAD_GTEST=on # Use CMake to just download Google Test
# On a machine that does not have an internet access
cmake ../ -DBUILD_TEST=on -DSKIP_DOWNLOAD_GTEST=on # Add other options you want to use
```

## Example

Example programs are located in [example/](example/)
* Basic examples
	* [simple.cpp](./example/simple.cpp)
    	* A simple example of allocating a vector container.

	* [vector_of_vectors.cpp](./example/vector_of_vectors.cpp)
    	* An example of nested (multi-level) containers.

	* [offset_pointer.cpp](./example/offset_pointer.cpp)
    	* An example code using the offset pointer to store pointer persistently.

	* [snapshot.cpp](./example/snapshot.cpp)
    	* An example code that snapshots and copies the snapshot files to a new place.

* Graph
	* [csr_graph.cpp](./example/csr_graph.cpp)
		* An example of the CSR graph data structure with Metall
	* [adjacency_list_graph.cpp](./example/adjacency_list_graph.cpp)
		* An example of the adjacency-lis data structure with Metall
	
* MPI (experimental implementation)
	* [mpi_create.cpp](./example/mpi_create.cpp) and [mpi_open.cpp](./example/mpi_open.cpp)
	* Metall does not support multi-process, i.e., there is no inter-process synchronization mechanism in Metall. Metall assumes that each process access a different memory region. The examples above shows how to use Metall with MPI.
