# Example

Example programs are located in [example](https://github.com/LLNL/metall/tree/develop/example)

To build examples see [build source files in Metall](./advanced/build_sources.md)

## Basic examples

* [simple.cpp](https://github.com/LLNL/metall/tree/develop/example/simple.cpp)
    * A simple example of allocating a vector container.

* [vector_of_vectors.cpp](https://github.com/LLNL/metall/tree/develop/example/vector_of_vectors.cpp)
    * An example of nested (multi-level) containers.

* [offset_pointer.cpp](https://github.com/LLNL/metall/tree/develop/example/offset_pointer.cpp)
    * An example code using the offset pointer to store pointer persistently.

* [snapshot.cpp](https://github.com/LLNL/metall/tree/develop/example/snapshot.cpp)
    * An example code that snapshots and copies the snapshot files to a new place.


## Graph

* [csr_graph.cpp](https://github.com/LLNL/metall/tree/develop/example/csr_graph.cpp)
    * An example of the CSR graph data structure with Metall
    
* [adjacency_list_graph.cpp](https://github.com/LLNL/metall/tree/develop/example/adjacency_list_graph.cpp)
    * An example of the adjacency-lis data structure with Metall
	
	
## MPI (experimental implementation)

* [mpi_create.cpp](https://github.com/LLNL/metall/tree/develop/example/mpi_create.cpp) and [mpi_open.cpp](https://github.com/LLNL/metall/tree/develop/example/mpi_open.cpp)

* Use Metall with MPI program (each process creates its own Metall object)

* Metall does not support multi-process, i.e., there is no inter-process synchronization mechanism in Metall. Metall assumes that each process access a different memory region. The examples above shows how to use Metall with MPI.
