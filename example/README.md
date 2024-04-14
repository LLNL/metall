# List of Examples

To build examples see a [page](https://metall.readthedocs.io/en/latest/advanced_build/cmake/) hosted on Read the Docs.

### Basic examples

* [simple.cpp](simple.cpp)

  * Allocating a vector container.

* [snapshot.cpp](snapshot.cpp)

  * Taking snapshot

* [offset_pointer.cpp](offset_pointer.cpp)

  * Using the offset pointer to store pointer persistently.


### STL Container with Custom Allocator

In the examples below, we occasionally use STL-compatible containers exist in [metall/container/](../include/metall/container/) for better readability.
Those containers are just alias of the corresponding containers in Boost.Container and do not have Metall-specific features.
Therefore, the data structures in the examples below are more like how to use STL containers with custom allocators rather than how to use containers with just Metall.

* [string.cpp](string.cpp)

  * STL string container with Metall.

* [vector_of_vectors.cpp](vector_of_vectors.cpp)

  * Nested (multi-level) containers.

* [multilevel_containers.cpp](multilevel_containers.cpp)

  * Another multi-level STL container example.

* [string_map.cpp](string_map.cpp)

  * How to use an STL map container of which key is STL string.

* [complex_map.cpp](complex_map.cpp)

  * A complex STL map container with Metall.

* [allocator_aware_type.cpp](allocator_aware_type.cpp)

  * How to implement your own container (data structure) that A) can be stored in another container; B) holds another container inside.


### Metall (advanced)

* [logger.cpp](logger.cpp)

  * How to use Metall's logger.

* [datastore_description.cpp](datastore_description.cpp)

  * Set and get datastore description

* [object_attribute.cpp](object_attribute.cpp)

  * How to access object attributes

* [object_attribute_api_list.cpp](object_attribute_api_list.cpp)

  * List of API to manipulate object attributes


### Metall's Original Container

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

* [fallback_allocator.cpp](fallback_allocator.cpp)

    * How to use [fallback_allocator](../include/metall/container/fallback_allocator.hpp)


### MPI (experimental implementation)

* [mpi_create.cpp](mpi_create.cpp) and [mpi_open.cpp](mpi_open.cpp)

* Use Metall with MPI program (each process creates its own Metall object).

* Metall does not support multi-process, i.e., there is no inter-process synchronization mechanism in Metall. Metall assumes that each process access a different memory region. The examples above shows how to use Metall with MPI.

* One can set a MPI CXX compiler to use by using 'MPI_CXX_COMPILE' CMake option.

### C API

* [c_api.cpp](c_api.c)
