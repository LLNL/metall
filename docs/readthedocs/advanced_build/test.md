# Test Metall for Development

Metall provides two main testing paths: the Google Test suite and a manual
persistence check based on the adjacency-list benchmark.

## Google Test

### Build

Configure the repository with `BUILD_TEST=ON`, then build it.

```bash
cmake -S . -B build -DBUILD_TEST=ON
cmake --build build
```

Google Test is downloaded automatically unless `SKIP_DOWNLOAD_GTEST=ON`.
See [the CMake build page](./cmake.md) for additional options.

### Run

```bash
ctest --test-dir build --output-on-failure
```

By default, the test programs create data stores under `/tmp/metall_test_dir`.

To change the location, set the `METALL_TEST_DIR` environment variable.

```bash
env METALL_TEST_DIR="/mnt/ssd/metall-tests" ctest --test-dir build --output-on-failure
```

## Manual Test

Metall also includes a manual persistence test under
[bench/adjacency_list](https://github.com/LLNL/metall/tree/master/bench/adjacency_list).
One program creates graph data and another reopens it, which is useful for
checking that data persists correctly across runs.

Build the benchmark targets first:

```bash
cmake -S . -B build -DBUILD_BENCH=ON
cmake --build build
```

Here is how to run the test with small data.

```bash
cd metall/build/bench/adjacency_list/
bash ../../../bench/adjacency_list/test/test.sh -d /path/to/store/data
```

Here is how to run the test with large data.

```bash
cd metall/build/bench/adjacency_list/
bash ../../../bench/adjacency_list/test/test_large.sh -d /path/to/store/data -v17
```

In `test_large.sh`, the input graph is generated on the fly with an R-MAT
generator. The `-vN` option controls the graph scale. For a given `N`, the
generated graph has `2^N` vertices and `16 x 2^N` undirected edges.
