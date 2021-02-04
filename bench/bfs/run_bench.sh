#!/usr/bin/env bash

# Usage
# cd metall/build/bench/bfs
# sh run_bench.sh -v 31 -f $((2**40)) -t 24 -s static[dynamic,10000] -g /dev/shm

# ----- Options----- #
V=17
FILE_SIZE=$((2**30))
BFS_ROOT=0
LOG_FILE_PREFIX="out_adj_bfs_bench"
NUM_THREADS=""
SCHEDULE=""
case "$OSTYPE" in
  darwin*)  GRAPH_DIR_ROOT="/tmp";;
  linux*)   GRAPH_DIR_ROOT="/dev/shm";;
esac
CHUNK_SIZE=$((2**20))
NO_CLEANING_FILES_AT_END=false

while getopts "v:f:r:l:m:t:s:g:n:c" OPT
do
  case $OPT in
    v) V=$OPTARG;;
    f) FILE_SIZE=$OPTARG;;
    r) BFS_ROOT=$OPTARG;;
    l) LOG_FILE_PREFIX=$OPTARG;;
    t) NUM_THREADS="env OMP_NUM_THREADS=${OPTARG}";;
    s) SCHEDULE="env OMP_SCHEDULE=${OPTARG}";;
    g) GRAPH_DIR_ROOT=$OPTARG;;
    n) CHUNK_SIZE=$OPTARG;;
    c) NO_CLEANING_FILES_AT_END=true;;
    :) echo  "[ERROR] Option argument is undefined.";;   #
    \?) echo "[ERROR] Undefined options.";;
  esac
done

# ----- Configurations ----- #
A=0.57
B=0.19
C=0.19
SEED=123
E=$((2**$((${V}+4))))
MAX_VERTEX_ID=$((2**${V}))

GRAPH_NAME="segment"
ADJ_LIST_KEY_NAME="adj_list"

case "$OSTYPE" in
  darwin*)
    INIT_COMMAND=""
    ;;
  linux*)
    case $HOSTNAME in
        dst-* | bertha* | altus*)
            INIT_COMMAND="/home/perma/drop_buffer_cache "
            ;;
        *) # Expect other LC machines
            INIT_COMMAND="srun --drop-caches=pagecache true"
            ;;
    esac ;;
esac

if [[ $MAX_VERTEX_ID -eq 0 ]]; then
    MAX_VERTEX_ID=$((2**${V}))
fi

make_dir() {
    if [ ! -d "$1" ]; then
        mkdir -p $1
    fi
}

LOG_FILE=""

try_to_get_compiler_ver() {
    strings $1 | grep "GCC" | tee -a ${LOG_FILE}
}

execute() {
    echo "Command: " "$@" | tee -a ${LOG_FILE}
    echo ">>>>>" | tee -a ${LOG_FILE}
    time "$@" | tee -a ${LOG_FILE}
    echo "<<<<<" | tee -a ${LOG_FILE}
}

get_system_info() {
    echo "" | tee -a ${LOG_FILE}

    echo "/proc/sys/vm/dirty_expire_centisecs" | tee -a ${LOG_FILE}
    cat /proc/sys/vm/dirty_expire_centisecs | tee -a ${LOG_FILE}
    echo "/proc/sys/vm/dirty_ratio" | tee -a ${LOG_FILE}
    cat /proc/sys/vm/dirty_ratio | tee -a ${LOG_FILE}
    echo "/proc/sys/vm/dirty_background_ratio" | tee -a ${LOG_FILE}
    cat /proc/sys/vm/dirty_background_ratio | tee -a ${LOG_FILE}
    echo "/proc/sys/vm/dirty_writeback_centisecs" | tee -a ${LOG_FILE}
    cat /proc/sys/vm/dirty_writeback_centisecs | tee -a ${LOG_FILE}

    df -lh | tee -a ${LOG_FILE}
    mount | tee -a ${LOG_FILE}

    echo "" | tee -a ${LOG_FILE}
}

run() {
    # ------------------------- #
    # Configure
    # ------------------------- #
    EXEC_NAME=$1
    LOG_FILE=${LOG_FILE_PREFIX}"_"${EXEC_NAME}".log"
    echo "" > ${LOG_FILE}

    GRAPH_DIR=${GRAPH_DIR_ROOT}/${EXEC_NAME}
    make_dir ${GRAPH_DIR}
    rm -f "${GRAPH_DIR}/*"

    # ------------------------- #
    # Start benchmark
    # ------------------------- #
    get_system_info

    echo "" | tee -a ${LOG_FILE}
    echo "----------------------------------------" | tee -a ${LOG_FILE}
    echo "Dynamic Graph Construction with" ${EXEC_NAME} | tee -a ${LOG_FILE}
    echo "----------------------------------------" | tee -a ${LOG_FILE}

    ${INIT_COMMAND}

    if [[ $OSTYPE = "linux"* ]]; then
        free -g  | tee -a ${LOG_FILE}
    fi

    exec_file_name="../adjacency_list/run_adj_list_bench_${EXEC_NAME}"
    try_to_get_compiler_ver ${exec_file_name}
    execute ${NUM_THREADS} ${SCHEDULE} ${exec_file_name} -o "${GRAPH_DIR}/${GRAPH_NAME}" -f ${FILE_SIZE} -s ${SEED} -v ${V} -e ${E} -a ${A} -b ${B} -c ${C} -r 1 -u 1

    ls -Rlsth ${GRAPH_DIR}"/" | tee -a ${LOG_FILE}

    df ${GRAPH_DIR}"/" | tee -a ${LOG_FILE}

    du -h ${GRAPH_DIR}"/" | tee -a ${LOG_FILE}

    echo "" | tee -a ${LOG_FILE}
    echo "----------------------------------------" | tee -a ${LOG_FILE}
    echo "BFS with" ${EXEC_NAME} | tee -a ${LOG_FILE}
    echo "----------------------------------------" | tee -a ${LOG_FILE}

    ${INIT_COMMAND}
    if [[ $OSTYPE = "linux"* ]]; then
        free -g  | tee -a ${LOG_FILE}
    fi
    exec_file_name="./run_bfs_bench_${EXEC_NAME}"
    try_to_get_compiler_ver ${exec_file_name}
    execute ${NUM_THREADS} ${SCHEDULE} ${exec_file_name} -g "${GRAPH_DIR}/${GRAPH_NAME}" -k ${ADJ_LIST_KEY_NAME} -r ${BFS_ROOT} -m ${MAX_VERTEX_ID}


    if ${NO_CLEANING_FILES_AT_END}; then
        echo "Do not delete the used directory"
    else
        echo "Delete the used directory"
        rm -rf ${GRAPH_DIR}
    fi
}

main() {
    run bip
    run metall
    #run metall_numa
}

main "$@"