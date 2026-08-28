# Why Metall Exists

Data-intensive applications often spend significant time loading raw data,
building in-memory indexes, and reshaping data into structures that are fast
for analytics. In many workflows, that preparation cost is higher than the
analytic itself.

Metall is designed for the cases where those data structures should survive a
single process lifetime. Instead of rebuilding them on every run, an
application can store them in a persistent heap and reopen them later.

## What Metall Provides

- A persistent heap for C++ objects backed by files and mapped into virtual
  memory with `mmap`.
- Support for both block storage and byte-addressable persistent memory, such
  as NVMe SSDs and persistent-memory devices.
- An API style based on Boost.Interprocess, which makes it possible to build
  allocator-aware user-defined types and containers in persistent storage.
- Snapshot support so applications can create explicit recovery points.
- Support for datasets that may be larger than DRAM because the mapped region
  can extend beyond physical memory.

## Important Things to Know

- Metall is a persistent allocator, not a transactional database. Durability is
  established at well-defined points such as clean shutdown or explicit
  snapshot creation.
- Objects stored in Metall must follow persistent-memory rules. In particular,
  data structures should use offset pointers and allocator-aware types instead
  of raw process-local pointer mechanisms.
- Applications typically keep one or more named root objects so they can reopen
  a datastore and rediscover the entry points of the object graph.
- Metall supports multithreaded use within a process. In multi-process
  workflows, each process is expected to manage its own Metall-managed data.

The following detail pages explain the practical constraints around pointers,
snapshots, and crash recovery.
