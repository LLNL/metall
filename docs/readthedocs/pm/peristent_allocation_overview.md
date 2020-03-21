
# Limitations To Store Objects Persistently

To store objects persistently, there are some limitations as listed below.

* Raw pointers
    * When store pointers persistently, raw pointers have to be replaced with offset pointers because there is no guarantee that backing-files are mapped to the same virtual memory address every time.
    * An offset pointer holds an offset between the address pointing at and itself.
* References
    * References have to be removed due to the same reason as raw pointers.
* Virtual Function and Virtual Base Class
    * As the virtual table also uses raw pointers, virtual functions and virtual base classes are not allowed.
* STL Containers
    * Some of STL containers' implementations do not work with Metall ([see detail](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.stl_container_requirements)).
    * We recommend using [containers implemented in Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess/allocators_containers.html#interprocess.allocators_containers.containers_explained.containers)
     as they fully support persistent allocations.

Some example programs that use Metall are listed [here](#example).

