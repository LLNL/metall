#!/usr/bin/env bash

export OMP_SCHEDULE="static"
export OMP_NUM_THREADS=96

# Create a new data store
./run_adj_list_bench_metall -o "/tmp/bench" -V "file1"

# '-A' Append mode
./run_adj_list_bench_metall -o "/tmp/bench" -V -A "file2"

# '-S:' Staging location
./run_adj_list_bench_metall -o "/tmp/bench" -V -S "./stg" "file1"

# Staging and Append
./run_adj_list_bench_metall -o "/tmp/bench" -V -A -S "./stg" "file2"

# Useful information
echo "Datastore"
ls -Rlsth ${DATASTORE_DIR}"/"
df ${DATASTORE_DIR}"/"
du -h ${DATASTORE_DIR}"/"


# Wikipedia dataset (this is not
# /p/lustre3/havoqgtu/wikipedia-enwiki-20170701/extracted_link/page_link_update_stream/list-