# List of Examples

To build examples see a [page](https://metall.readthedocs.io/en/latest/advanced_build/cmake/) hosted on Read the Docs.

### Basic examples

* [simple.cpp](simple.cpp)
    * Allocating a vector container.

* [vector_of_vectors.cpp](vector_of_vectors.cpp)
    * Nested (multi-level) containers.

* [offset_pointer.cpp](offset_pointer.cpp)
    * Using the offset pointer to store pointer persistently.

* [snapshot.cpp](snapshot.cpp)
    * Taking snapshot

* [string.cpp](string.cpp)
    * How to use the STL string with Metal

* [string_map.cpp](string_map.cpp)
    * How to use a STL map container of which key is STL string

* [complex_map.cpp](complex_map.cpp)
    * A complex STL map container with Metall

* [multilevel_containers.cpp](multilevel_containers.cpp)
    * A multi-level STL container with Metall

* [datastore_description.cpp](datastore_description.cpp)
  * Set and get datastore description

* [object_attribute.cpp](object_attribute.cpp)
  * How to access object attributes

* [object_attribute_api_list.cpp](object_attribute_api_list.cpp)
  * List of API to manipulate object attributes


### Metall Container

* [metall_containers.cpp](metall_containers.cpp)
  * List of containers that use Metall at the default allocators.
  
### Graph

* [csr_graph.cpp](csr_graph.cpp)
    * A CSR graph data structure (two 1D arrays) with Metall

* [adjacency_list_graph.cpp](adjacency_list_graph.cpp)
    * An adjacency-lis data structure (multi-level STL container) with Metall


### Concurrent Data Structure (experimental implementation)

* [static_mutex.cpp](static_mutex.cpp)
    * How to use our [mutex_lock function](../include/metall/utility/mutex.hpp)

* [concurrent_map.cpp](concurrent_map.cpp)
    * How to use our [persistent concurrent_map class](../include/metall/container/concurrent_map.hpp)


### Fallback Allocator

* [fallback_allocator_adaptor.cpp](fallback_allocator_adaptor.cpp)
    * How to use [fallback_allocator_adaptor](../include/metall/utility/fallback_allocator_adaptor.hpp)


### MPI (experimental implementation)

* [mpi_create.cpp](mpi_create.cpp) and [mpi_open.cpp](mpi_open.cpp)

* Use Metall with MPI program (each process creates its own Metall object).

* Metall does not support multi-process, i.e., there is no inter-process synchronization mechanism in Metall. Metall assumes that each process access a different memory region. The examples above shows how to use Metall with MPI.

* One can set a MPI CXX compiler to use by using 'MPI_CXX_COMPILE' CMake option.

## C API
* [c_api.cpp](c_api.c)
