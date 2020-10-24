#!/usr/bin/env bash

# cd metall/build/bench/adjacency_list
# sh ../../../bench/adjacency_list/run_bench.sh

# ----- Options----- #
VERTEX_SCALE=17
FILE_SIZE=$((2 ** 30)) # File size for Boost.Interprocess
LOG_FILE_PREFIX="out_adj_bench"
NUM_THREADS=""
SCHEDULE=""
case "$OSTYPE" in
darwin*) DATASTORE_DIR_ROOT="/tmp" ;;
linux*) DATASTORE_DIR_ROOT="/dev/shm" ;;
esac
CHUNK_SIZE=$((2 ** 20))
NO_CLEANING_FILES_AT_END=false
UMAP_PAGESIZE=""
EXEC_NAME="metall" # "run_adj_list_bench_${EXEC_NAME}" is the execution file

while getopts "v:f:l:t:s:d:n:cp:E:" OPT; do
  case $OPT in
  v) VERTEX_SCALE=$OPTARG ;;
  f) FILE_SIZE=$OPTARG ;;
  l) LOG_FILE_PREFIX=$OPTARG ;;
  t) NUM_THREADS="env OMP_NUM_THREADS=${OPTARG}" ;;
  s) SCHEDULE="env OMP_SCHEDULE=${OPTARG}" ;;
  d) DATASTORE_DIR_ROOT=$OPTARG ;;
  n) CHUNK_SIZE=$OPTARG ;;
  c) NO_CLEANING_FILES_AT_END=true ;;
  p) UMAP_PAGESIZE="env UMAP_PAGESIZE=${OPTARG}" ;;
  E) EXEC_NAME=$OPTARG ;;
  :) echo "[ERROR] Option argument is undefined." ;; #
  \?) echo "[ERROR] Undefined options." ;;
  esac
done

# ----- Configurations ----- #
A=0.57
B=0.19
C=0.19
RND_SEED=123
DATASTORE_NAME="metall_adjlist_bench"

#case "$OSTYPE" in
#  darwin*)
#    DROP_PAGECACHE_COMMAND=""
#    ;;
#  linux*)
#    case $HOSTNAME in
#        dst-* | bertha* | altus*)
#            DROP_PAGECACHE_COMMAND="/home/perma/drop_buffer_cache "
#            ;;
#        *) # Expect other LC machines
#            DROP_PAGECACHE_COMMAND="srun --drop-caches=pagecache true"
#            ;;
#    esac ;;
#esac

make_dir() {
  if [ ! -d "$1" ]; then
    mkdir -p $1
  fi
}

try_to_get_compiler_ver() {
  local exec_file_name=$1

  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "Compiler information in" ${exec_file_name} | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  strings ${exec_file_name} | grep "GCC" | tee -a ${LOG_FILE}
}

execute() {
  echo "Command: " "$@" | tee -a ${LOG_FILE}
  echo ">>>>>" | tee -a ${LOG_FILE}
  time "$@" | tee -a ${LOG_FILE}
  echo "<<<<<" | tee -a ${LOG_FILE}
}

execute_simple() {
  echo "$@" | tee -a ${LOG_FILE}
  "$@" | tee -a ${LOG_FILE}
}

log_header() {
  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "$@" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
}

get_system_info() {
  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "System information" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}

  echo "free -g" | tee -a ${LOG_FILE}
  if [[ $OSTYPE == "linux"* ]]; then
    free -g | tee -a ${LOG_FILE}
  fi

  echo "" | tee -a ${LOG_FILE}
  echo "/proc/sys/vm/dirty_expire_centisecs" | tee -a ${LOG_FILE}
  cat /proc/sys/vm/dirty_expire_centisecs | tee -a ${LOG_FILE}
  echo "/proc/sys/vm/dirty_ratio" | tee -a ${LOG_FILE}
  cat /proc/sys/vm/dirty_ratio | tee -a ${LOG_FILE}
  echo "/proc/sys/vm/dirty_background_ratio" | tee -a ${LOG_FILE}
  cat /proc/sys/vm/dirty_background_ratio | tee -a ${LOG_FILE}
  echo "/proc/sys/vm/dirty_writeback_centisecs" | tee -a ${LOG_FILE}
  cat /proc/sys/vm/dirty_writeback_centisecs | tee -a ${LOG_FILE}

  echo "" | tee -a ${LOG_FILE}
  echo "" | tee -a ${LOG_FILE}
  df -lh | tee -a ${LOG_FILE}
  mount | tee -a ${LOG_FILE}

  echo "" | tee -a ${LOG_FILE}
}

run() {
  # ------------------------- #
  # Configure
  # ------------------------- #
  LOG_FILE=${LOG_FILE_PREFIX}"_"${EXEC_NAME}".log"
  echo "" >${LOG_FILE} # Reset the log file

  local datastore_dir=${DATASTORE_DIR_ROOT}/${EXEC_NAME}
  make_dir ${datastore_dir}
  rm -f "${datastore_dir}/*" # Erase old data if they exist

  # ------------------------- #
  # Show system inf
  # ------------------------- #
  get_system_info
  #    ${DROP_PAGECACHE_COMMAND}

  # ------------------------- #
  # Benchmark main
  # ------------------------- #
  local exec_file_name="../adjacency_list/run_adj_list_bench_${EXEC_NAME}"
  echo "Run the benchmark using" ${exec_file_name} | tee -a ${LOG_FILE}

  try_to_get_compiler_ver ${exec_file_name}

  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "Start benchmark" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  local datastore_path=${datastore_dir}/${DATASTORE_NAME}
  local num_edges=$((2 ** $((${VERTEX_SCALE} + 4))))
  execute ${NUM_THREADS} ${SCHEDULE} ${UMAP_PAGESIZE} ${exec_file_name} -o ${datastore_path} -f ${FILE_SIZE} -s ${RND_SEED} -v ${VERTEX_SCALE} -e ${num_edges} -a ${A} -b ${B} -c ${C} -r 1 -u 1 -n ${CHUNK_SIZE} -V

  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "Datastore information" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  ls -Rlsth ${datastore_dir}"/" | tee -a ${LOG_FILE}
  df ${datastore_dir}"/" | tee -a ${LOG_FILE}
  du -h ${datastore_dir}"/" | tee -a ${LOG_FILE}

  if ${NO_CLEANING_FILES_AT_END}; then
    echo "Do not delete the used directory"
  else
    echo "Delete the used directory"
    rm -rf ${datastore_dir}
  fi
}

main() {
  run
}

main "$@"
