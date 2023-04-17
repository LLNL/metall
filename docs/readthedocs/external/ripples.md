# Ripples & Metall

## Introduction

This page describes technical details about the ongoing collaboration with the [Ripples](https://github.com/pnnl/ripples), a software framework to study the Influence Maximization problem.

Ripples needs to perform multiple data construction steps. However, those steps account for a large amount of time in real workloads.

Ripples can persistently store a portion of the constructed data structures using the standard file I/O operation.
However, it stores only simple data structures (e.g., std::vector) because it causes additional overheads in terms of coding and performance to assemble and disassemble complex data structures (e.g., std::map).

Metall allows applications to store the C++ containers easily. It also has multiple features for large-scale data management, such as the snapshot, parallel data copy, and user-level mmap implementation support for better data locality.

This collaboration aims at leveraging Metall in Ripples' actual workload. To investigate how Metall can improve Ripples', we have integrated Metall into Ripples.

### Integrating Metall into Ripples

Ripples uses STL containers for its internal data structures.
Thanks to Metall's rich C++ API (which was mainly inherited from the Boost.Interprocess library),
we were able to integrate Metall into Ripples following the C++ standard syntax.

Specifically, we changed the original code so that internal data structures can accept a custom memory allocator instead of the default one.
On the other hand, we did not have to modify the core parts of graph construction or analytics code.

Such reasonable code modification exhibits Metall's high adaptability.

## Build Example

Here we describe how to build Ripples with Metall to persistently store Ripples graph data.

Tested Environment:

- Linux
- Python 3.7
- GCC 10 (or later)
- CMake 3.23 (or later)

The following instructions are tested with the latest version of Ripples at the time of writing (commit ID: da08b3e759642a93556f081169c61607354ecd3e).

```shell
# Get source code
git clone git@github.com:pnnl/ripples.git
cd ripples
git checkout da08b3e759642a93556f081169c61607354ecd3e

# Apply the patch file (download from the link under this code block)
git apply ripples-metall.patch

# Hereafter, build instructions are described in the original Ripples:
# https://github.com/pnnl/ripples/blob/master
# (please enable Metall as described on the page above)
#
# Here, we show example build commands for the convenience of the reader.

# Set up Python environment, if not available
# For example:
pip install --user pipenv
pip install --user conan==1.59.0
# If needed, configure PATH, for example:
# export PATH="$HOME/.local/bin:$PATH"

# Set up Python environment
pipenv --python 3.7
pipenv install
pipenv shell

# Create a conan profile
conan profile new default --detect
conan profile update settings.compiler.libcxx=libstdc++11 default
conan profile update env.CC=$(which gcc) default
conan profile update env.CXX=$(which g++) default

# Install dependencies
conan create conan/waf-generator user/stable
conan create conan/trng user/stable
conan create conan/metall user/stable

# Enable the Metall mode
conan install --install-folder build . --build fmt -o metal=True

# Build
./waf configure --enable-metall build_release
```

Download a patch file from here [ripples-metall.patch](./ripples-metall.patch).

## Run Example

```shell
./build/release/tools/imm -i test-data/karate.tsv -e 0.5 -k 100 -d LT --parallel --metall-store-dir=/mnt/ssd/graph
```

- Reads edge data from test-data/karate.tsv
- Stores the constructed graph in /mnt/ssd/graph

## Allocate RRRSets Using Metall

Ripples + Metall has another mode that allocates intermediate data (called RRRSets) using Metall.

To enable the mode, define `ENABLE_METALL_RRRSETS` macro (e.g., insert `#define ENABLE_METALL_RRRSETS` at the beginning of `tools/imm.cc`).

RRRSet (intermediate data) is allocated in `/dev/shm` (tmpfs) by default. To change the location, modify line 82 `include/ripples/generate_rrr_sets.h` and re-build the program (`./waf configure --enable-metall build_release`).