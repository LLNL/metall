## Use Umap in Metall

Metall supports [Umap](https://github.com/LLNL/umap) as a replacement for the system mmap.

Link Umap library and include its header files and define ```METALL_USE_UMAP``` at compile.
An actual build command would be something like:

```bash
g++ -std=c++17 -lstdc++fs your_program.cpp \
-I/path/to/metall/include -I/path/to/boost/include \
-I/path/to/umap/include -L/path/to/umap/lib -lumap -DMETALL_USE_UMAP
```

In addition,  
our CMake file has an option `UMAP_ROOT=/umap/install/root/path` to use Umap instead of the system mmap.


### Restriction with Umap

Metall cannot free DRAM space (page buffer managed by Umap) and backing file space since Umap does not have such capabilities.
