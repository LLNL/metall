Metall employs the API developed by Boost.Interprocess (BIP).

Although BIP has been developed as an interprocess communication library, it has a collection of APIs that are useful for persistent memory allocators.

The APIs allow developers to allocate not only contiguous memory regions like malloc(3) but also complex custom data structures, including the C++ STL
containers, in persistent memory.

To work with the C++ STL containers, Boost.Interprocess has an allocator class that is compatible with the STL allocator.
It also offers an interface similar to a key-value store so that applications can find already allocated objects when reattaching the previously created application data.

Metall supports multi-thread.
In multi-process environment, Metall assumes each process allocates its own Metall object(s).

## Main APIs in Metall

Here, we list Metall's main APIs.

```C++
// The main header file
#include <metall/metall.hpp>

// The main class of Metall
class metall::manager;

// ---------- Constructors ---------- //
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

// ---------- Allocation ---------- //
//  Allocates n bytes
void* manager.allocate(size_t n);

//  Deallocates the allocated memory
void manager.deallocate(void *addr)

// ---------- STL allocator ---------- //
// Returns an STL allocator object for type T
allocator_type<T> manager.get_allocator<T>()

// ---------- Attributed Object ---------- //
// Allocates and constructs an object of T with arguments args.
// Also stores the allocated memory address with key name
T* manager.construct<T, Args>(char* name)(Args... args)

// Finds an already constructed object with key name
T* manager.find<T>(char* name)

// Destroys a previously created named and unique instance
// Calls the destructor and frees the memory.
bool manager.destroy(char* name)

// Destroys a object (named, unique, or anonymous) by its address.
// Calls the destructor and frees the memory.
bool manager.destroy_ptr<T>(T* ptr)

// ---------- Attributed Object (Metall original) ---------- //
// Returns the name of an attributed object
const char_type *manager.get_instance_name<T>(const T *ptr)

// Returns the kind of an object. One of the following value is returned:
// instance_kind::named_kind
// instance_kind::unique_kind
// instance_kind::anonymous_kind
instance_kind manager.get_instance_kind<T>(const T *ptr)

// Returns the length of an object
size_type manager.get_instance_length<T>(const T *ptr)

// Checks if the type of an object is T.
bool manager.is_instance_type<T>(const T *ptr)

// Gets the description of an object
bool manager.get_instance_description(const T *ptr, std::string *description)

// Sets a description to an object
bool manager.set_instance_description(const T *ptr, const std::string& description)

// ---------- Snapshot (Metall original) ---------- //
// Takes a snapshot of the current datastore.
bool manager.snapshot(const char *destination_dir_path)

// ---------- Utilities (Metall original) ---------- //
// Check if a datastore exists and is consistent
// (i.e., it was closed properly in the previous run).
static bool metall::manager::consistent(const char *dir_path)

// Copies datastore
static bool metall::manager::copy(const char *source_dir_path, const char *destination_dir_path)

// Removes datastore synchronously
static bool metall::manager::remove(const char *dir_path)

// Gets the version number of the Metall that created the current datastore.
version_type manager.get_version()

// Gets the version of the Metall that created a datastore
static version_type metall::get_version(const char_type *dir_path)

// ---------- Data store description ---------- //
// Sets a description
bool set_description(const std::string &description)
static bool metall::set_description(const char *dir_path, const std::string &description)

// Gets a description
bool get_description(std::string *description)
static bool metall::get_description(const char *dir_path, std::string *description)
```

Example programs are located in [example](https://github.com/LLNL/metall/tree/master/example).

## FUll API document

The full API document is available [here](https://software.llnl.gov/metall/api/).

To generate the full API document locally using Doxygen:

```bash
cd metall
mkdir build_doc
cd build_doc
doxygen ../docs/Doxyfile.in
```