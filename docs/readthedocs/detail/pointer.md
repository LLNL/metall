# Offset Pointers

When a Metall datastore is reopened, its backing files may be mapped at a
different virtual address from the previous run. That means a raw pointer value
stored inside the persistent object graph is not stable across executions.

To solve this, Metall uses an offset pointer instead of a raw pointer for
persistent links between objects. An offset pointer stores the relative
distance to the pointed-to object, so it remains valid even when the datastore
is remapped at a different virtual address.

Metall's `offset_ptr` is an alias of
[Boost.Interprocess `offset_ptr`](https://www.boost.org/doc/libs/release/doc/html/interprocess/offset_ptr.html).

## Rules for Types Stored in Metall

- Use `metall::offset_ptr<T>` for pointers that are part of persistent state.
- Do not store C++ references in persistent objects.
- Avoid virtual functions and virtual base classes in persisted types because
  those features rely on process-local pointer state.
- Use allocator-aware types for dynamically allocated members such as strings,
  vectors, maps, and nested containers.
- Keep at least one named root object so the application can reopen the
  datastore and call `find` to recover the object graph.

Raw pointers are still fine as temporary local variables after an object has
been found or constructed. The restriction is about what gets stored in the
persistent data structure itself.

## Containers

Not every implementation of the standard library containers works correctly
with Boost.Interprocess-style allocators and offset pointers. For the detailed
requirements, see the
[Boost.Interprocess container notes](https://www.boost.org/doc/libs/release/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.stl_container_requirements).

In practice, the safest choices are:

- [Boost.Container](https://www.boost.org/doc/libs/release/doc/html/container.html)
  containers.
- Metall's allocator-aware container facilities.
- Simple user-defined types whose pointer ownership and allocators are explicit.
