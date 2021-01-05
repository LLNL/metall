#!/usr/bin/env bash

NUM_ALLOCS=1000000
FILE="/tmp/segment"
LOG_FILE_PREFIX="out_simple_allocation_bench_"

rm -rf ${FILE}*
./run_simple_allocation_bench_stl -n ${NUM_ALLOCS} | tee ${LOG_FILE_PREFIX}"stl.log"

rm -rf ${FILE}*
./run_simple_allocation_bench_bip -n ${NUM_ALLOCS} -o ${FILE} | tee ${LOG_FILE_PREFIX}"bip.log"

rm -rf ${FILE}*
./run_simple_allocation_bench_metall -n ${NUM_ALLOCS} -o ${FILE} | tee ${LOG_FILE_PREFIX}"metall.log"