Although Boost.Interprocess (BIP) has been developed as an interprocess communication library, it has a collection of APIs that are useful for persistent memory allocators.

The APIs allow developers to allocate not only contiguous memory regions like malloc(3) but also complex custom data structures, including the C++ STL
containers, in persistent memory.

BIP also offers an interface similar to a key-value store so that applications can find already allocated objects when reattaching the previously created application data.

BIP also has an interface that returns an object of the Standard Template Library (STL) style allocator.

Some basic APIs in Metall are:

```C++
//  Allocates n bytes
void* allocate(size_t n);

//  Deallocates the allocated memory
void deallocate(void *addr)

// Allocates and constructs an object of T with arguments args.
// Also stores the allocated memory address with key name
T* construct<T, Args>(char* name)(Args... args)

// Finds an already constructed object with key name
T* find<T>(char* name)
    
// Returns an STL allocator object for type T
metall_stl_allocator<T> get_allocator<T>()
```

Here is a simple example of allocating a STL vector container ([simple.cpp](https://github.com/LLNL/metall/tree/develop/example/simple.cpp)).

More examples are listed in [this page](../example.md). 