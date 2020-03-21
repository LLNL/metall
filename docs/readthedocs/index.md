Metall (Meta Allocator)
====================

* Provides rich memory allocation interfaces for C++ applications that use persistent memory devices to persistently store heap data on such devices.
* Creates files in persistent memory and maps them into virtual memory space so that users can access the mapped region just as normal memory regions allocated in DRAM.
* To provide persistent memory allocation, Metall employs concepts and APIs developed by [Boost.Interprocess](https://www.boost.org/doc/libs/1_69_0/doc/html/interprocess.html).
* Supports multi-threa
* Also provides a space-efficient snapshot/versioning leveraging reflink copy mechanism in filesystem. In case reflink is not supported, Metall automatically falls back to regular copy.
* Example programs that use Metall are listed [here](./example.md).