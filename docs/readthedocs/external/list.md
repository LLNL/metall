# Open Source Projects Using Metall

## miniVite

MiniVite is benchmark in the ECP ExaGraph suite that implements a single phase of the Louvain method for community detection.

miniVite has a mode that uses Metall to store a graph into persistent memory.

The miniVite version that works with Metall comes with a CMake file.

For building and running miniVite with Metall see details [here](https://github.com/ECP-ExaGraph/miniVite/tree/metallds2#minivite--metall-and-umap).

## Ripples

Ripples is a software framework to study the Influence Maximization problem.

See [detail](./ripples.md).


## HavoqGT

HavoqGT (Highly Asynchronous Visitor Queue Graph Toolkit) is a framework for expressing asynchronous vertex-centric graph algorithms.

All graph data is allocated by Metall.

https://github.com/LLNL/havoqgt

## saltatlas

saltatlas DNND is a distributed NNDescent application.
saltatlas DNND leverages Metall to store k-NN index, which requires a heavy construction time.

https://github.com/LLNL/saltatlas