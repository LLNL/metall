#!/bin/bash

# USAGE
# cd metall/build/bench/adjacency_lsit/
# sh ../../../bench/adjacency_lsit/test/test.sh

DATA=../../../bench/adjacency_lsit/test/data/edge_list_rmat_s10_

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  num_elements=$(< ${file1} wc -l)
  if [ ${num_elements} -eq 0 ]; then
    echo "<< ${file1} is empty!! >>"
    exit
  fi

  echo "Sort the dumped edges"
  sort -k 1,1n -k2,2n < ${file1} > /tmp/file1_sorted
  sort -k 1,1n -k2,2n < ${file1} > /tmp/file2_sorted

  echo "Compare the dumped edges"
  diff /tmp/file1_sorted /tmp/file2_sorted > /tmp/file_diff
  num_diff=$(< /tmp/file_diff wc -l)

  if [ ${num_diff} -eq 0 ]; then
    echo "<< Passed the test!! >>"
  else
    err "<< Failed the test!! >>"
  fi
  echo ""

  # /bin/rm ${file1} ${file2} /tmp/file1_sorted /tmp/file2_sorted /tmp/file_diff
}

check_program_exit_status() {
  ret=$?

  if [ $ret -ne 0 ]; then
    err "<< The program did not finished correctly!! >>"
    exit
  fi
}

main() {
  echo "CreateTest"
  ./run_adj_list_bench_metall -o /tmp/segment -f $((2**26)) -d /tmp/adj_out ${DATA}*
  check_program_exit_status
  cat ${DATA}* >> /tmp/adj_ref
  compare "/tmp/adj_out" "/tmp/adj_ref"

  echo ""
  echo "OpenTest"
  ./test/open_metall -o /tmp/segment -d /tmp/adj_out_reopen
  check_program_exit_status
  cat ${DATA}* >> /tmp/adj_ref
  compare "/tmp/adj_out_reopen" "/tmp/adj_ref"
  /bin/rm /tmp/segment*
}

main "$@"