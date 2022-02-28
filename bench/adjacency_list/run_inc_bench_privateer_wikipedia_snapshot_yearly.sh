#!/bin/bash

export OMP_SCHEDULE="static"
export OMP_NUM_THREADS=96

INPUT_BASE_PATH=$1
INPUT_FILES_LIST=$2
DATASTORE_DIR=$3
FILE_GRANULARITY=$4

# mkdir /p/lustre3/youssef2/metall_privateer_results_archive/results_snapshot_map_of_deque/${FILE_GRANULARITY}
first_file=$(head -n 1 ${INPUT_FILES_LIST})

echo "Adding first chunk"

# srun --drop-caches=pagecache true
./run_adj_list_bench_metall_snapshot -o ${DATASTORE_DIR} -V ${INPUT_BASE_PATH}/${first_file}
du --apparent-size ${DATASTORE_DIR}"/"
# rm /dev/shm/sem.*
# echo "Snapshots storage usage"
# du -hs --apparent-size ${DATASTORE_DIR}_*
# echo "-----------------------"
# cp ${DATASTORE_DIR}/metall_datastore/segment/version_metadata/_metadata /p/lustre3/youssef2/metall_privateer_results_archive/results_snapshot_map_of_deque/${FILE_GRANULARITY}/_metadata_Wikipedia_part0000
echo "Done adding first chunk"

# echo ${INPUT_BASE_PATH}/${first_file}

while IFS= read -r line
do
echo "Checking $line"
if [ "$line" != "${first_file}" ]; then
   echo "Adding ${line}"
   # srun --drop-caches=pagecache true
   ./run_adj_list_bench_metall_snapshot -o ${DATASTORE_DIR} -V -A ${INPUT_BASE_PATH}/$line
   du --apparent-size ${DATASTORE_DIR}"/"
   # rm /dev/shm/sem.*
   # echo "Snapshots storage usage"
   # du -hs --apparent-size ${DATASTORE_DIR}_*
   # echo "-----------------------" 
   # cp ${DATASTORE_DIR}/metall_datastore/segment/version_metadata/_metadata /p/lustre3/youssef2/metall_privateer_results_archive/results_snapshot_map_of_deque/${FILE_GRANULARITY}/_metadata_${line}
   echo "Done adding ${line}"
fi
done < "${INPUT_FILES_LIST}"

# Useful information
echo "Datastore"
ls -Rlsth ${DATASTORE_DIR}"/"
df ${DATASTORE_DIR}"/"
du -h --apparent-size ${DATASTORE_DIR}"/"
echo "Sizes of snapshots"
du -h --apparent-size ${DATASTORE_DIR}_*
# cp -r ${DATASTORE_DIR}/metall_datastore/segment/version_metadata /g/g90/youssef2/metall-1/build_privateer_2/bench/adjacency_list/version_metadata_${FILE_GRANULARITY}
rm -rf ${DATASTORE_DIR}
mkdir -p /p/lustre3/youssef2/metall_privateer_snapshot_final_hpec/${FILE_GRANULARITY}_yearly_final_no_copies
cp -r ${DATASTORE_DIR}_* /p/lustre3/youssef2/metall_privateer_snapshot_final_hpec/${FILE_GRANULARITY}_yearly_final_no_copies
rm -rf ${DATASTORE_DIR}_*
