# Tutorial Programs

* Allocate memory using Metall
    * This code does not reattach data

* Allocate memory and reattach it
    * Allocate int object
    * Allocate a struct object
    
* Metall with STL Container
    * How to use Metall with STL containers
    
* Offset Pointer

* Container of containers
    * How to use multi-layer STL container (i.e., vector<vector<int>>) with a custom allocator

* Allocator-aware Data Structure
    * How to design allocator-aware data structure
    * vector (1D array) data structure
    * matrix (2D array) data structure
    
* Snapshot
  * Snapshot
  * Consistency check
  
# Build

## Required
- GCC 8.1 or more (8.3 or more is recommended due to early implementation of the Filesystem library).

## Example  
```bash
# Download Boost (Boost C++ Libraries 1.64 or more is required)
# One can skip this step if Boost is already available.
wget https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz
tar xvf boost_1_75_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_75_0

g++ -std=c++17 tutorial_program.cpp -lstdc++fs -I../../include -I${BOOST_ROOT}
```
