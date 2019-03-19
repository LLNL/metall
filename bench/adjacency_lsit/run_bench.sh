#!/usr/bin/env bash

# cd metall/build/bench/adjacency_lsit
# sh ../../../bench/adjacency_lsit/run_bench.sh

v=17
a=0.57
b=0.19
c=0.19
seed=123
file_size=$((2**30))
log_file_prefix="out_adj_list_bench"
num_threads=""
schedule=""

while getopts "v:f:l:t:s:" OPT
do
  case $OPT in
    v) v=$OPTARG;;
    f) file_size=$OPTARG;;
    l) log_file_prefix=$OPTARG;;
    t) num_threads="env OMP_NUM_THREADS=${OPTARG}";;
    s) schedule="env OMP_SCHEDULE=${OPTARG}";;
    :) echo  "[ERROR] Option argument is undefined.";;
    \?) echo "[ERROR] Undefined options.";;
  esac
done

e=$((2**$((${v}+4))))

case "$OSTYPE" in
  darwin*)  OUT_PATH="/tmp";;
  linux*)   OUT_PATH="/dev/shm";;
esac

case "$OSTYPE" in
  darwin*)
    INIT_COMMAND=""
    ;;
  linux*)
    PRE_COMMAND="taskset -c 1 "
    case $HOSTNAME in
        dst-*)
            INIT_COMMAND="/home/perma/drop_buffer_cache "
            ;;
        *) # Expect other LC machines
            INIT_COMMAND="srun --drop-caches=pagecache true"
            ;;
    esac ;;
esac

function run() {
    echo ""
    echo "--------------------"
    echo ""

    EXEC_NAME=$1
    LOG_FILE=${log_file_prefix}"_"${EXEC_NAME}".log"

    rm -f ${OUT_PATH}/segment*
    ${INIT_COMMAND}
    ${num_threads} ${schedule} ./run_adj_list_bench_${EXEC_NAME} -o "${OUT_PATH}/segment" -f ${file_size} -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1 | tee ${LOG_FILE}
    ls -lsth ${OUT_PATH}"/" | tee -a ${LOG_FILE}
}

run stl
run bip
#run bip_extend
run metall