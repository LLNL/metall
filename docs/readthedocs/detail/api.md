Metall employs the API developed by Boost.Interprocess (BIP).

Although BIP has been developed as an interprocess communication library, it has a collection of APIs that are useful for persistent memory allocators.

The APIs allow developers to allocate not only contiguous memory regions like malloc(3) but also complex custom data structures, including the C++ STL
containers, in persistent memory.

To work with the C++ STL containers, Boost.Interprocess has an allocator class that is compatible with the STL allocator.
It also offers an interface similar to a key-value store so that applications can find already allocated objects when reattaching the previously created application data.

Metall supports multi-thread.
In multi-process environment, Metall assumes each process allocates its own Metall object(s).

## Main APIs in Metall

```C++
// The main header file
#include <metall/metall.hpp>

// The main class of Metall
class metall::manager;

// -------------------- Constructors -------------------- //
// Opens an existing data store.
manager(open_only_t, const char *base_path,
        const kernel_allocator_type &allocator = kernel_allocator_type())


// Opens an existing data store with the read only mode.
// Write accesses will cause segmentation fault.
manager(open_read_only_t, const char *base_path,
        const kernel_allocator_type &allocator = kernel_allocator_type())


// Creates a new data store (an existing data store will be overwritten).
manager(create_only_t, const char *base_path,
        const kernel_allocator_type &allocator = kernel_allocator_type())

// -------------------- Allocation -------------------- //
//  Allocates n bytes
void* metall::manager.allocate(size_t n);

//  Deallocates the allocated memory
void metall::manager.deallocate(void *addr)

// -------------------- STL allocator -------------------- //
// Returns an STL allocator object for type T
allocator_type<T> metall::manager.get_allocator<T>()

// -------------------- Object Allocation (following Boost.Interprocess) -------------------- //
// Allocates and constructs an object of T with arguments args.
// Also stores the allocated memory address with key name
T* metall::manager.construct<T, Args>(char* name)(Args... args)

// Finds an already constructed object with key name
T* metall::manager.find<T>(char* name)

// Destroys a previously created unique instance
bool metall::manager.destroy(char* name)

// -------------------- Snapshot (Metall original) -------------------- //      
// Snapshot the entire data
bool metall::manager.snapshot(const char *destination_dir_path)

// -------------------- Utilities (Metall original) -------------------- //
// Check if the backing data store exists and is consistent
// (i.e., it was closed properly in the previous run).
static bool metall::manager::consistent(const char *dir_path)

// Copies backing data store synchronously.
static bool metall::manager::copy(const char *source_dir_path, const char *destination_dir_path)

// Remove a data store synchronously.
static bool metall::manager::remove(const char *dir_path)

```

Example programs are located in [example](https://github.com/LLNL/metall/tree/master/example).

## Generate API document using Doxygen

A Doxygen configuration file is [here](https://github.com/LLNL/metall/tree/master/docs/Doxyfile.in).

To generate API document:

```bash
cd metall
mkdir build_doc
cd build_doc
doxygen ../docs/Doxyfile.in
```