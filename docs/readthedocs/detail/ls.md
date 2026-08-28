# Metall datastore 'ls'

`datastore_ls` is a utility program that lists object metadata stored in a
Metall datastore.

`mpi_datastore_ls` is the corresponding utility for a Metall MPI datastore
created with the
[Metall MPI adaptor](https://github.com/LLNL/metall/blob/master/include/metall/utility/metall_mpi_adaptor.hpp).
`mpi_datastore_ls` itself is not an MPI program.

These tools are useful when you want to:

- confirm that named root objects were created,
- inspect object counts, offsets, type IDs, and descriptions,
- debug what is actually present in a datastore before reopening it in code.

## Build

```bash
cmake -S . -B build -DBUILD_UTILITY=ON
cmake --build build --target datastore_ls mpi_datastore_ls
```

To install the utilities under a chosen prefix:

```bash
cmake --install build --prefix /install/path
```

## Synopsis

```c++
datastore_ls [/path/to/datastore]
mpi_datastore_ls [/path/to/datastore] [MPI rank number]
```

## Example

```bash
/install/path/bin/datastore_ls /path/to/metall/datastore
[Named Object]
|   Name |  Length |   Offset |              Type-ID |          Description |
----------------------------------------------------------------------------
|    obj |       1 |        0 |  6253375586064260614 |  description example |

[Unique Object]
|  Name: typeid(T).name() |  Length |   Offset |               Type-ID |  Description |
--------------------------------------------------------------------------------------
|                       c |       1 |        8 |  10959529184379665549 |              |
|       St6vectorIiSaIiEE |       1 |  4194304 |  11508737342576383696 |              |

[Anonymous Object]
|  Length |   Offset |              Type-ID |  Description |
-----------------------------------------------------------
|       1 |       16 |  6253375586064260614 |              |
|     100 |  6291456 |  6253375586064260614 |              |
```

The output is grouped by named, unique, and anonymous objects. For named
objects, the `Name` column shows the key that applications can use with
`manager.find()` to recover the object after reopening the datastore.
