## Coarse-grained Persistence Policy vs Fine-grained Persistence Policy

### Coarse-grained Persistence Policy

Metall employs snapshot consistency, an explicit coarse-grained persistence policy in which persistence is guaranteed only when the heap is saved in a *snapshot* to the backing store (backing files).

The snapshot is created when the destructor or a snapshot method in Metall is invoked.
Those methods flush the application data and the internal management data in Metall to the backing store.

If an application crashes before Metall's destructor finishes successfully,
there is a possibility of inconsistency between the memory mapping and the backing files. 

To protect application data from this hazard,
the application must duplicate the backing files
before reattaching the data either through the snapshot method or else using a copy command in the system.



### Fine-grained Persistence Policy

In contrast, [libpmemobj](https://pmem.io/pmdk/libpmemobj/) in the [Persistent Memory Development Kit (PMDK)](https://pmem.io/pmdk/) builds on [Direct Access (DAX)](https://www.kernel.org/doc/Documentation/filesystems/dax.txt) and is designed to provide fine-grained persistence.

Fine-grained persistence is highly useful (or almost necessary) to implement transactional object stores, leveraging new byte-addressable persistent memory fully, e.g., Intel Optane DC Persistent Memory.

However, fine-grained persistence requires fine-grained cache-line flushes to the persistent media, which can incur an unnecessary overhead for applications that do not require such fine-grained consistency.
It is also not possible to efficiently support such fine-grained consistency on more traditional NVMe devices.
