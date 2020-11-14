## Use Metall with Your Program

Metall consists of only header files and requires some header files in Boost C++ Libraries.

Metall is available at: <https://github.com/LLNL/metall>.

All core files exist under [metall/include/metall/](https://github.com/LLNL/metall/tree/master/include/metall).

To build your program with Metall,
all you have to do is just setting include paths such as '-I' or CPLUS_INCLUDE_PATH.

For example,
```bash
g++ -std=c++17 your_program.cpp -lstdc++fs -I/path/to/metall/include -I/path/to/boost/include
```
Note that one might need to link stdc++fs library to use the Filesystem library in C++17.


## Install Using Spack

Metall package is also available on [Spack](https://spack.io/).

To install Metall using Spack, type ```spack install metall```.

As Metall requires Boost C++ Libraries, Spack also installs a proper
version of Boost C++ Libraries automatically, if needed.

As Spack's load command configures environmental values properly, one
can avoid specifying include paths to build a program with Metall.
For instance:

```bash
spack load metall
g++ -std=c++17 your_program.cpp -lstdc++fs
```


## Required to Build Metall

- GCC 8.1 or more (8.3 or more is recommended due to early implementation of the Filesystem library).
- Boost C++ Libraries 1.64 or more (build is not required; needs only
  their header files).

### Unofficial Support For Clang
Clang can be used instead of GCC to build Metall.
However, we haven't tested it intensively.
Also, Boost C++ Libraries 1.69 or more may be required
if one wants to build Metall with Clang + CUDA.


## Compile-time Options
There are some compile-time options as follows to configure the behavior of Metall:

* METALL_DISABLE_FREE_FILE_SPACE
	* If defined, Metall does not free file space

* METALL_FREE_SMALL_OBJECT_SIZE_HINT=*N*
	* Experimental option
	* If defined, Metall tries to free space when an object equal or larger than *N* bytes is deallocated.
