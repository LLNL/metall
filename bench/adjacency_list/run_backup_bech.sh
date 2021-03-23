#!/bin/bash

sudo sh -c "echo 90 > /proc/sys/vm/dirty_ratio"
sudo sh -c "echo 80 > /proc/sys/vm/dirty_background_ratio"
sudo sh -c "echo 300000000 > /proc/sys/vm/dirty_expire_centisecs"
ulimit -n 4096

export OMP_SCHEDULE="static"
export OMP_NUM_THREADS=96

# Create a new data store
DATASTORE_DIR="/mnt/ssd/"
DATASTORE_PATH="${DATASTORE_DIR}/graph"
DATASET_PREFIX="/tmp/wiki_sorted_by_src_dataset/x"

rm -rf ${DATASTORE_DIR}*

echo ""
echo ""
echo "-----"
echo "No. 00000"
echo "-----"
./run_adj_list_backup_bench -o ${DATASTORE_PATH} -n$((2**26)) "${DATASET_PREFIX}00000"
ls -lsth ${DATASTORE_DIR}

for i in {1..31}; do
  FILE_NO=`printf %05d $i`
  echo ""
  echo ""
  echo "-----"
  echo "No. ${FILE_NO}"
  echo "-----"
  DATASET_PATH="${DATASET_PREFIX}${FILE_NO}"
  ./run_adj_list_backup_bench -o ${DATASTORE_PATH} -A -n$((2**26)) ${DATASET_PATH}
  ls -lsth ${DATASTORE_DIR}
done

# Useful information
echo "Datastore"
ls -Rlsth ${DATASTORE_DIR}
df ${DATASTORE_DIR}"/"
du -h ${DATASTORE_DIR}"/"

