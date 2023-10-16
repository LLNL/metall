# Compile-Time Options

There are some compile-time options (C/C++ macro) as follows to configure the behavior of Metall:

- METALL_DEFAULT_CAPACITY=*bytes*
  - The default capacity of a segment/datastore.
  - This value is used when a user does not specify the capacity of a datastore when creating it.

- METALL_VERBOSE_SYSTEM_SUPPORT_WARNING
  - If defined, Metall shows warning messages at compile time if the system does not support important features.

- METALL_DISABLE_CONCURRENCY
  - Disable concurrency support in Metall. This option is useful when Metall is used in a single-threaded application.
  - If this macro is defined, applications must not call Metall concurrently from multiple threads.
  - Even if this option is enabled, Metall still uses multiple threads for background tasks, such as synchronizing segment files.

- METALL_USE_SORTED_BIN
  - If defined, Metall stores addresses in sorted order in the bin directory.
  - This option enables Metall to use memory space more efficiently, but it increases the cost of the bin directory operations.

- METALL_FREE_SMALL_OBJECT_SIZE_HINT=*bytes*
  - If defined, Metall tries to free space when an object equal to or larger than the specified bytes is deallocated.
  - Will be rounded up to a multiple of the page size internally.


**Macros for the segment storage manager:**

- METALL_SEGMENT_BLOCK_SIZE=*bytes*
  - The segment block size.
  - Metall allocates a backing file with this size.

- METALL_DISABLE_FREE_FILE_SPACE
  - If defined, Metall does not free file space.

- METALL_USE_ANONYMOUS_NEW_MAP
  - If defined, Metall uses anonymous memory mapping instead of file mapping when creating a new map region.