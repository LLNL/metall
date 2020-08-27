Although Boost.Interprocess (BIP) has been developed as an interprocess communication library, it has a collection of APIs that are useful for persistent memory allocators.

The APIs allow developers to allocate not only contiguous memory regions like malloc(3) but also complex custom data structures, including the C++ STL
containers, in persistent memory.

To work with the C++ STL containers, Boost.Interprocess has an allocator class that is compatible with the STL allocator.
It also offers an interface similar to a key-value store so that applications can find already allocated objects when reattaching the previously created application data.

Some basic APIs in Metall are:

```C++
// The main header file
#include <metall/metall.hpp>

// The main class of Metall
class metall::manager;

//  Allocates n bytes
void* metall::manager.allocate(size_t n);

//  Deallocates the allocated memory
void metall::manager.deallocate(void *addr)

// Allocates and constructs an object of T with arguments args.
// Also stores the allocated memory address with key name
T* metall::manager.construct<T, Args>(char* name)(Args... args)

// Finds an already constructed object with key name
T* metall::manager.find<T>(char* name)

// Destroys a previously created unique instance
bool metall::manager.destroy(char* name)
      
// Returns an STL allocator object for type T
allocator_type<T> metall::manager.get_allocator<T>()

// Snapshot the entire data
bool metall::manager.snapshot(const char *destination_dir_path)

// Check if the backing data store is consistent,
// i.e. where it was closed properly during the previous execution
static bool metall::manager.consistent(const char *dir_path)
```

Metall supports multi-thread.
In multi-process environment, Metall assumes each process allocates its own Metall object.

Some examples are listed [here](./example.md).