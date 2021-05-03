# Compile-time Options

There are some compile-time options (macro) as follows to configure the behavior of Metall:


- METALL_DISABLE_FREE_FILE_SPACE
    - If defined, Metall does not free file space


- METALL_DEFAULT_VM_RESERVE_SIZE=*bytes*
    - The default virtual memory reserve size
    - An internally defined value is used if 0 is specified
    - Wll be rounded up to a multiple of the page size internally 


- METALL_INITIAL_SEGMENT_SIZE=*bytes*
    - The initial segment size
    - Use the internally defined value if 0 is specified
    - Wll be rounded up to a multiple of the page size internally


- METALL_FREE_SMALL_OBJECT_SIZE_HINT=*bytes*
    - Experimental option
    - If defined, Metall tries to free space when an object equal or larger than specified bytes is deallocated
    - Wll be rounded up to a multiple of the page size internally
  