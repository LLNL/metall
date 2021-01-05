## Metall datastore 'ls'

`datastore_ls` is a utility program that lists object attributes in a Metall data store.

`mpi_datastore_ls` is available for Metall MPI data store,
which is created by [Metall MPI Adaptor](https://github.com/LLNL/metall/blob/master/include/metall/utility/metall_mpi_adaptor.hpp).
`mpi_datastore_ls` is not a MPI program.

### Synopsis
```c++
datastore_ls [/path/to/datastore]
mpi_datastore_ls [/path/to/datastore] [MPI rank number]
```

### Example
```bash
$ cmake [option]...
$ make datastore_ls
$ make install
$/install/path/bin/datastore_ls /path/to/metall/datastore                                                                                 
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