## Background

Data-intensive applications play an essential role across many real-world data science domains.
Often, these applications require storing data beyond a single process lifetime.
Data often requires transformation into analytic-specific data structures to perform the analytic with reasonable execution time.  
The task of ingesting data, indexing and partitioning data in preparation of running an analytic, is often more expensive than the analytic itself.

The promise of persistent memory is that, once constructed, data structures can be re-analyzed and updated beyond the lifetime of a single execution, and new forms of persistent memory are increasing the viability of processing complex data analytics.


## Metall

* Enables applications to allocate heap-based objects into both block-storage and byte-addressable persistent memories, just like main-memory
	* e.g., NVMe SSD and Intel Optane DC Persistent Memory

* Leverages a memory-mapped file mechanism (mmap)
	* Applications can access mapped area as if it were regular memory
	* mmap can map files bigger than the DRAM capacity

* Incorporates the state-of-the-art allocation algorithms
	* Some key ideas from [SuperMalloc](https://dl.acm.org/doi/10.1145/2887746.2754178) 

* Provides the API developed by Boost.Interprocess
  * Boost.Interprocess is an interprocess communication library
  * Useful for allocating C++ custom data structures in persistent memory

* Employs a coarse-grained consistency model, allowing the application to determine when it is appropriate to create durable snapshots of the persistent heap