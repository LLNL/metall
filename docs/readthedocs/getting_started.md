## Use Metall with Your Program

Metall consists of only header files and requires some header files in Boost C++ Libraries.

Metall is available at: <https://github.com/LLNL/metall>.

All core files exist under [metall/include/metall/](https://github.com/LLNL/metall/tree/develop/include/metall).

To build your program with Metall,
all you have to do is just setting include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,
```bash
g++ -std=c++17 -lstdc++fs your_program.cpp -I/path/to/metall/include -I/path/to/boost/include
```
Note GCC requires linking stdc++fs to use the Filesystem library in C++17.


## Insall Using Spack
Metall package is also available on [Spack](https://spack.io/).

To install Metall using Spack, just type ```spack install metall```.

As Metall requires Boost C++ Libraries, Spack also install a proper (latest) version of Boost C++ Libraries automatically.

As Spack configure environmental values properly, you can avoid specifying include paths to build your program. 
For instance,
```bash
spack load metall
g++ -std=c++17 -lstdc++fs your_program.cpp
```


## Required

 - GCC 8.1 or more.
 - Boost C++ Libraries 1.64 or more (build is not required; needs only their header files).


## Compile-time Options
There are some compile-time options as follows to configure the behavior of Metall:

* METALL_DISABLE_FREE_FILE_SPACE
	* If defined, Metall does not free file space

* METALL_FREE_SMALL_OBJECT_SIZE_HINT=*N*
	* Experimental option
	* If defined, Metall tries to free space when an object equal or larger than *N* bytes is deallocated.
