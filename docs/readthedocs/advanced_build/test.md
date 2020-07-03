# Test Metall for Development

There are two types of tests programs in Metall.


## Google Test


### Build

To run tests using Google Test,
please see [this page](./example_test_bench.md) about building the test programs.


### Run

```bash
make test
```

The test programs make data stores into `/tmp` by default.

To change the location, use an environmental value 'METALL_TEST_DIR'.

```bash
env METALL_TEST_DIR="/mnt/ssd/" make test
```


## Manual Test
There is another test program in Metall's repository.  
The test program uses [Adjacency List Benchmark](https://github.com/LLNL/metall/tree/develop/bench/adjacency_list/).
One program creates a graph data and another program opens it.
This test is useful to make sure that Metall can store data persistently.


Here is how to run the test with small data.
```bash
cd metall/build/bench/adjacency_list/
sh ../../../bench/adjacency_list/test/test.sh -d/path/to/store/data/store
```


Here is how to run the test with large data.
```bash
cd metall/build/bench/adjacency_list/
sh ../../../bench/adjacency_list/test/test_large.sh -d/path/to/store/data/store
```

Please edit the script directly to change data size.