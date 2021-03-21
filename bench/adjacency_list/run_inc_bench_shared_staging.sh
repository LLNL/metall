#!/usr/bin/env bash

export OMP_SCHEDULE="static"
export OMP_NUM_THREADS=96

# Create a new data store
DATASTORE_DIR=$1
srun --drop-caches=pagecache true
./run_adj_list_bench_metall -o ${DATASTORE_DIR} -V -S "/dev/shm/bench_wikipedia" "/p/lustre3/youssef2/Wikipedia_filtered/32_partitions/Wikipedia_part0000"
echo "Deleting Staging Directory..."
time rm -rf /dev/shm/bench_wikipedia 

for i in `seq 1 9`; do
   srun --drop-caches=pagecache true
  ./run_adj_list_bench_metall -o ${DATASTORE_DIR} -V -A -S "/dev/shm/bench_wikipedia" /p/lustre3/youssef2/Wikipedia_filtered/32_partitions/Wikipedia_part000$i;
  echo "Deleting Staging Directory..."
  time rm -rf /dev/shm/bench_wikipedia
done

for i in `seq 10 31`; do
   srun --drop-caches=pagecache true
  ./run_adj_list_bench_metall -o ${DATASTORE_DIR} -V -A -S "/dev/shm/bench_wikipedia" /p/lustre3/youssef2/Wikipedia_filtered/32_partitions/Wikipedia_part00$i;
  echo "Deleting Staging Directory..."
  time rm -rf /dev/shm/bench_wikipedia
done
# '-A' Append mode
# ./run_adj_list_bench_metall -o "/tmp/bench" -V -A "file2"

# '-S:' Staging location
# ./run_adj_list_bench_metall -o "/tmp/bench" -V -S "./stg" "file1"

# Staging and Append
# ./run_adj_list_bench_metall -o "/tmp/bench" -V -A -S "./stg" "file2"

# Useful information
echo "Datastore"
ls -Rlsth ${DATASTORE_DIR}"/"
df ${DATASTORE_DIR}"/"
du -h ${DATASTORE_DIR}"/"


# Wikipedia dataset (this is not
# /p/lustre3/havoqgtu/wikipedia-enwiki-20170701/extracted_link/page_link_update_stream/list-
