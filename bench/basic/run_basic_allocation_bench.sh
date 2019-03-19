#!/usr/bin/env bash

MIN_ALLOC_SIZE=8
MAX_ALLOC_SIZE=$((2**13))
NUM_ALLOCS=1000000
FILE="/dev/shm/segment"
LOG_FILE_PREFIX="out_basic_alloc_bench_"

INIT_COMMAND="${HOME}/../perma/drop_buffer_cache"
PRE_COMMAND="taskset -c 1 "

rm -f ${FILE}*
${INIT_COMMAND}
${PRE_COMMAND} ./run_basic_allocation_bench_stl ${MIN_ALLOC_SIZE} ${MAX_ALLOC_SIZE} ${NUM_ALLOCS} | tee ${LOG_FILE_PREFIX}"stl.log"

rm -f ${FILE}*
${INIT_COMMAND}
${PRE_COMMAND} ./run_basic_allocation_bench_bip ${MIN_ALLOC_SIZE} ${MAX_ALLOC_SIZE} ${NUM_ALLOCS} ${FILE} | tee ${LOG_FILE_PREFIX}"bip.log"

rm -f ${FILE}*
${INIT_COMMAND}
${PRE_COMMAND} ./run_basic_allocation_bench_metall ${MIN_ALLOC_SIZE} ${MAX_ALLOC_SIZE} ${NUM_ALLOCS} ${FILE} | tee ${LOG_FILE_PREFIX}"metall.log"