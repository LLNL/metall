# Welcome to Metall Tutorial ICS'21!

In this tutorial, we introduce Metall, a persistent memory allocator designed to provide developers with an API to allocate custom C++ data structures in both block-storage and byte-addressable persistent memories (e.g., NVMe SSD and Intel Optane DC Persistent Memory).

An often overlooked but common theme among the variety of data analytics platforms is the need to persist data beyond a single process lifecycle.
For example, data analytics applications usually perform data ingestion tasks, which index and partition data with analytics-specific data structures before performing the analysis.

However, the data ingestion task is often more expensive than the analytic itself, and the same or derived data is re-ingested frequently (e.g., running multiple analytics to the same data, developing/debugging analytics programs).
The promise of persistent memory is that, once constructed, data structures can be re-analyzed and updated beyond the lifetime of a single execution.

Thanks to the recent notable performance improvements and cost reductions in non-volatile memory (NVRAM) technology, we believe that leveraging persistent memory in this way brings significant benefits to data analytics applications.

## Agenda

### Section 1: Lecture (1 hour)
#### Background
- Persistent memory
- Leveraging persistent memory in data analytics

#### Metall 101
- API
- Internal architecture
- Performance evaluation

### Section 2: Application Case Studies  (1 hour)
- Exploratory data analytics using Metall
- GraphBLAS Template Library + Metall

### Section 3: Hands-on session  (1 hour)

- LLNL staff can stay online to work with anyone wishing to experiment with provided example code
- Build environments for Linux and OSX will be provided

---

# 0. Setting Up Environment for the Hands-on Session

We recommend starting setting up one's environment before
the hands-on session because some steps may take a long time.

## 0-1. Machine

Metall is designed to work on Linux machines.
It also works on macOS (some internal performance optimizations are disabled).


***Google Cloud***

One also could use a public cloud system, such as [Google Cloud](https://cloud.google.com/).
Here is an example of creating an account on Google Cloud: [use_googlecloud.pdf](use_googleclould.pdf).


## 0-2. Compiler (GCC)

Install GCC 8.1 or more. One could also use Clang or Apple clang; however, GCC is recommended.

## 0-3. Boost C++ Libraries

Metall depends on  [Boost C++ Libraries](https://www.boost.org/) (1.64 or more is required).
Only the header files are used by Metall.
One **does not have to build** it.

Download Boost from [here](https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz) (a download process will start automatically)
and uncompress it (e.g., double-click the downloaded file).

Or, on a terminal:
```bash
# Download Boost C++ Libraries
wget https://boostorg.jfrog.io/artifactory/main/release/1.75.0/source/boost_1_75_0.tar.gz
tar xvf boost_1_75_0.tar.gz
```

# 1. Lecture Materials

* [Metall 101 slides](metall_101.pdf)

# 2. Hands-on

## 2-1. Recommended Flow

![](tutorial_flow.png)


## 2-2. Build and Run

**Required**

- GCC 8.1 or more

**Build example**
```bash
# Download Boost C++ Libraries (1.64 or more is required)
# One can skip this step if Boost is already available
wget https://dl.bintray.com/boostorg/release/1.75.0/source/boost_1_75_0.tar.gz
tar xvf boost_1_75_0.tar.gz
export BOOST_ROOT=$PWD/boost_1_75_0

git clone https://github.com/LLNL/metall
cd metall/tutorial/ics21
g++ -std=c++17 [tutorial_program.cpp] -lstdc++fs -I../../include -I${BOOST_ROOT}
# If one gets an error related to pthread, please add '-pthread' at the end of the command above

# All tutorial programs do not take command-line options
./a.out
```


## 2-3. (optional) Build using Clang or Apple clang

Clang (or Apple clang) could be used instead of GCC to build Metall.
However, please note that we haven't tested it intensively.


**On macOS >= 10.15 or Linux**

```bash
# Remove "-lstdc++fs" option
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT}
```


**On macOS < 10.15**

The C++17 <filesystem> library is not available on macOS < 10.15.
One has to stop using C++17 <filesystem> library in Metall.
If METALL_DISABLE_CXX17_FILESYSTEM_LIB macro is defined, Metall uses its own file system operation implementation.

```bash
clang++ -std=c++17 [tutorial_program.cpp] -I../../include -I${BOOST_ROOT} -DMETALL_DISABLE_CXX17_FILESYSTEM_LIB
```