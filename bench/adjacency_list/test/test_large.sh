#!/bin/bash

# USAGE
# cd metall/build/bench/adjacency_list/
# sh ../../../bench/adjacency_list/test/test_large.sh

# ----- Configuration ----- #
file_size=$((2**30))
v=17
a=0.57
b=0.19
c=0.19
seed=123
e=$((2**$((${v}+4))))

case "$OSTYPE" in
  darwin*)  out_path="/tmp";;
  linux*)   out_path="/dev/shm";;
esac

data_store_path="${out_path}/metall_test_dir"
adj_list_dump_file="${out_path}/dumped_edge_list"
edge_dump_file="${out_path}/ref_edge_list"
# --------------- #

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  echo "Number of dumped edges"
  wc -l "${file1}"
  wc -l "${file2}"

  echo "Sort the dumped edges"
  tmp_sorted_file1=${out_path}/tmp_sorted1
  tmp_sorted_file2=${out_path}/tmp_sorted2
  sort -k 1,1n -k2,2n < "${file1}" > ${tmp_sorted_file1}
  sort -k 1,1n -k2,2n < "${file2}" > ${tmp_sorted_file2}

  echo "Compare the dumped edges"
  tmp_diff_file="${out_path}/tmp_diff_file"
  # Be careful " " and "\t"
  diff ${tmp_sorted_file1} ${tmp_sorted_file2} > ${tmp_diff_file}
  num_diff=$(< ${tmp_diff_file} wc -l)

  if [[ ${num_diff} -eq 0 ]]; then
    echo "<< Passed the test!! >>"
  else
    err "<< Failed the test!! >>"
    exit
  fi
  echo ""

  /bin/rm -f ${tmp_sorted_file1} ${tmp_sorted_file2} ${tmp_diff_file}
  echo ""
}

check_program_exit_status() {
  ret=$?

  if [[ $ret -ne 0 ]]; then
    err "<< The program did not finished correctly!! >>"
    exit
  fi
}

main() {

  ./run_adj_list_bench_metall -o ${data_store_path} -f ${file_size} -d ${adj_list_dump_file} -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1 -D ${edge_dump_file}
  check_program_exit_status
  echo ""

  compare ${adj_list_dump_file} ${edge_dump_file}

  /bin/rm -rf ${adj_list_dump_file}

  # ----- Reopen the file for persistence test----- #
  ./test/open_metall -o ${data_store_path} -d ${adj_list_dump_file}
  check_program_exit_status
  echo ""

  compare ${adj_list_dump_file} ${edge_dump_file}

  /bin/rm -f ${adj_list_dump_file} ${edge_dump_file}
  /bin/rm -rf ${data_store_path}
}

main "$@"