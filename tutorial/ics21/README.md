# Welcome to Metall Tutorial ICS'21!

In this tutorial, we introduce Metall, a persistent memory allocator designed to provide developers with an API to allocate custom C++ data structures in both block-storage and byte-addressable persistent memories (e.g., NVMe SSD and Intel Optane DC Persistent Memory).

An often overlooked but common theme among the variety of data analytics platforms is the need to persist data beyond a single process lifecycle.
For example, data analytics applications usually perform data ingestion tasks, which index and partition data with analytics-specific data structures before performing the analysis.

However, the data ingestion task is often more expensive than the analytic itself, and the same or derived data is re-ingested frequently (e.g., running multiple analytics to the same data, developing/debugging analytics programs).
The promise of persistent memory is that, once constructed, data structures can be re-analyzed and updated beyond the lifetime of a single execution.

Thanks to the recent notable performance improvements and cost reductions in non-volatile memory (NVRAM) technology, we believe that leveraging persistent memory in this way brings significant benefits to data analytics applications.

## Agenda

### Section 1: Lecture (1.5 hour)

#### Background
- Persistent memory
- Leveraging persistent memory in data analytics

#### Metall 101 & Hands-on

- API
- Internal architecture
- Hands-on
- Performance evaluation


### Section 2: Application Case Studies  (1 hour)

- Exploratory data analytics using Metall
- GraphBLAS Template Library + Metall


### Section 3: Q&A and Extra Hands-on Session  (0.5—1 hour)

- LLNL staff stay online to answer questions and work with anyone who wishes to experiment with provided example code


## Tutorial Materials

* [Lecture slides](metall_101.pdf)
* [Hands-on Examples](../hands_on)