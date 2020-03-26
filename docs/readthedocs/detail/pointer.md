## Offset Pointer

Applications have to take care of some restrictions regarding pointers to store objects in persistent memory.
Applications cannot use raw pointers for data members in data structures stored in persistent memory
because there is no guarantee that backing-files are mapped to the same virtual memory addresses every time.

To fix the problem, the **offset pointer** has to be used instead of the raw pointer.
An offset pointer holds an offset between the address pointing at and itself so that it can always point to the same location regardless of the VM address it is mapped.
Additionally, references, virtual functions, and virtual base classes have to be removed since those mechanisms also use raw pointers internally.

The [offset pointer in Metall](https://github.com/KIwabuchi/metall/blob/develop/include/metall/offset_ptr.hpp) is just an alias of [offset pointer in Boost.Interprocess](https://www.boost.org/doc/libs/release/doc/html/interprocess/offset_ptr.html).


## STL Container

Unfortunately, some implementations of the STL container do not work with Boost.Interprocess and Metall due to offset pointer and some other reasons ([see detail](https://www.boost.org/doc/libs/release/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.stl_container_requirements)).
We recommend applications use containers in [Boost.Container](https://www.boost.org/doc/libs/release/doc/html/container.html).

