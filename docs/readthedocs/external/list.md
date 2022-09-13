# Open Source Projects Using Metall

## Collaboration Work with the ECP ExaGraph Project
### miniVite

miniVite is a proxy app that implements a single phase of Louvain method in distributed memory for graph community detection.

miniVite has a mode that uses Metall to store a graph in persistent memory to reuse the data and reduce the overall analytics workload.

For building and running miniVite with Metall,
see the details located [here](https://github.com/ECP-ExaGraph/miniVite/tree/metallds2#minivite--metall-and-umap).

### Ripples

Ripples is a software framework to study the Influence Maximization problem developed at Pacific Northwest National Laboratory.

To build Riplles with Metall, see the details located [here](./ripples.md).

## HavoqGT

HavoqGT (Highly Asynchronous Visitor Queue Graph Toolkit) is a framework for expressing asynchronous vertex-centric graph algorithms.

Same as MiniVite, HavoqGT uses Metall to store a graph in persistent memory to reuse the data and reduce the overall analytics workload.

https://github.com/LLNL/havoqgt

## saltatlas

saltatlas DNND is a distributed NNDescent application.
saltatlas DNND leverages Metall to store k-NN index, which requires a heavy construction time.

https://github.com/LLNL/saltatlas