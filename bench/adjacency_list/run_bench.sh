#!/usr/bin/env bash

# ------------------------- #
# Usage
# ------------------------- #
# cd metall/build/bench/adjacency_list
# sh run_bench.sh [options]

# ------------------------- #
# Options
# ------------------------- #
VERTEX_SCALE=17        # RMAT graph SCALE
CHUNK_SIZE=$((2 ** 20)) # #of edges/items to insert at each iteration
FILE_SIZE=$((2 ** 30)) # File size for Boost.Interprocess and MEMKIND (pmem kind)
LOG_FILE_PREFIX="out_adj_bench"
NUM_THREADS=""
OMP_SCHEDULE="env OMP_SCHEDULE=static"
case "$OSTYPE" in
darwin*) DATASTORE_DIR_ROOT="/tmp" ;;
linux*) DATASTORE_DIR_ROOT="/dev/shm" ;;
esac
NO_CLEANING_FILES_AT_END=false
UMAP_PAGESIZE=""
EXEC_NAME="metall" # "run_adj_list_bench_${EXEC_NAME}" is the execution file

while getopts "v:f:l:t:s:d:n:cp:E:" OPT; do
  case $OPT in
  v) VERTEX_SCALE=$OPTARG ;;
  f) FILE_SIZE=$OPTARG ;;
  l) LOG_FILE_PREFIX=$OPTARG ;;
  t) NUM_THREADS="env OMP_NUM_THREADS=${OPTARG}" ;;
  s) OMP_SCHEDULE="env OMP_SCHEDULE=${OPTARG}" ;;
  d) DATASTORE_DIR_ROOT=$OPTARG ;;
  n) CHUNK_SIZE=$OPTARG ;;
  c) NO_CLEANING_FILES_AT_END=true ;;
  p) UMAP_PAGESIZE="env UMAP_PAGESIZE=${OPTARG}" ;;
  E) EXEC_NAME=$OPTARG ;;
  :) echo "[ERROR] Option argument is undefined." ;; #
  \?) echo "[ERROR] Undefined options." ;;
  esac
done

# ------------------------- #
# Configurations
# ------------------------- #
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

# ------------------------- #
# Utility functions
# ------------------------- #
# Makes a directory if it does not exit
make_dir() {
  if [ ! -d "$1" ]; then
    mkdir -p $1
  fi
}

# Executes a command, showing some information
execute() {
  echo "Command: " "$@" | tee -a ${LOG_FILE}
  echo ">>>>>" | tee -a ${LOG_FILE}
  time "$@" | tee -a ${LOG_FILE}
  echo "<<<<<" | tee -a ${LOG_FILE}
}

# Executes a command, showing some information
execute_simple() {
  echo "$@" | tee -a ${LOG_FILE}
  "$@" | tee -a ${LOG_FILE}
  echo "" | tee -a ${LOG_FILE}
}

# Logs a string with a heading style
log_header() {
  echo "" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "$@" | tee -a ${LOG_FILE}
  echo "----------------------------------------" | tee -a ${LOG_FILE}
  echo "" | tee -a ${LOG_FILE}
}

# Tries to get used compiler version from a given executable
try_to_get_compiler_info() {
  local exec_file_name=$1
  log_header "Compiler information in ${exec_file_name}"
  execute_simple sh -c "strings ${exec_file_name} | grep GCC"
}

# Logs some system information
log_system_info() {
  if [[ $OSTYPE == "linux"* ]]; then
    log_header "DRAM Usage"
    execute_simple free -g

    log_header "VM Configuration"
    execute_simple cat /proc/sys/vm/dirty_expire_centisecs
    execute_simple cat /proc/sys/vm/dirty_ratio
    execute_simple cat /proc/sys/vm/dirty_background_ratio
    execute_simple cat /proc/sys/vm/dirty_writeback_centisecs
  fi

  log_header "Storage Information"
  execute_simple df -lh
  execute_simple mount
}

# ------------------------- #
# Main
# ------------------------- #
main() {
  #  ----- Setup ----- #
  log_header "Benchmark Setup"
  LOG_FILE=${LOG_FILE_PREFIX}"_"${EXEC_NAME}".log"
  echo "" >${LOG_FILE} # Reset the log file

  local datastore_dir=${DATASTORE_DIR_ROOT}/${EXEC_NAME}
  make_dir ${datastore_dir}
  execute_simple rm -f "${datastore_dir}/*" # Erase old data if they exist

  # ----- Show system info ----- #
  log_system_info
  #    ${DROP_PAGECACHE_COMMAND}

  # ----- Benchmark main ----- #
  local exec_file_name="../adjacency_list/run_adj_list_bench_${EXEC_NAME}"

  try_to_get_compiler_info ${exec_file_name}

  log_header "Start benchmark"
  local datastore_path=${datastore_dir}/${DATASTORE_NAME}
  local num_edges=$((2 ** $((${VERTEX_SCALE} + 4))))
  execute ${NUM_THREADS} ${OMP_SCHEDULE} ${UMAP_PAGESIZE} ${exec_file_name} -o ${datastore_path} -f ${FILE_SIZE} -s ${RND_SEED} -v ${VERTEX_SCALE} -e ${num_edges} -a ${A} -b ${B} -c ${C} -r 1 -u 1 -n ${CHUNK_SIZE} -V

  log_header "Datastore information"
  execute_simple ls -Rlsth ${datastore_dir}"/"
  execute_simple df ${datastore_dir}"/"
  execute_simple du -h ${datastore_dir}"/"

  if ${NO_CLEANING_FILES_AT_END}; then
    echo "Do not delete the used directory"
  else
    echo "Delete the used directory"
    execute_simple rm -rf ${datastore_dir}
  fi
}

main "$@"
