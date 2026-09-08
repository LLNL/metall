# Coarse-grained Persistence Policy vs Fine-grained Persistence Policy

## Coarse-grained Persistence Policy

Metall uses an explicit, coarse-grained persistence model based on snapshot
consistency. In this model, application updates are not treated as durable one
write at a time. Instead, persistence is established when the application saves
the heap to backing files at defined points.

In practice, the current state becomes durable when a Metall manager is closed
cleanly or when the application explicitly creates a snapshot. At those points,
Metall flushes both application data and its internal management data.

### What Users Should Assume

- Updating an object in memory does not immediately mean the update is durable.
- A clean shutdown or an explicit snapshot is the normal boundary for durable
  state.
- If a process crashes before close or snapshot completes, the live datastore
  may be inconsistent.
- After an unclean shutdown, check the datastore with
  `metall::manager::consistent()` and recover from an earlier snapshot or copy
  if needed.

This is a good fit for applications that periodically establish stable recovery
points and do not need per-update transactional durability.

## Fine-grained Persistence Policy

In contrast, [libpmemobj](https://pmem.io/pmdk/libpmemobj/) in the [Persistent Memory Development Kit (PMDK)](https://pmem.io/pmdk/) builds on [Direct Access (DAX)](https://www.kernel.org/doc/Documentation/filesystems/dax.txt) and is designed to provide fine-grained persistence.

Fine-grained persistence is highly useful (or almost necessary) to implement transactional object stores, leveraging new byte-addressable persistent memory fully, e.g., Intel Optane DC Persistent Memory.

However, fine-grained persistence requires cache-line-level flushes to
persistent media, which can introduce overhead for applications that do not
need that level of consistency. It is also difficult to provide the same model
efficiently on conventional NVMe storage.
