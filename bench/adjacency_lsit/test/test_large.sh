#!/bin/bash

# USAGE
# cd metall/build/bench/adjacency_lsit/
# sh ../../../bench/adjacency_lsit/test/test_large.sh

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
out_name="dumped_edge_list"
out_ref="ref_edge_list"
# --------------- #

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  echo "Number of dumped edges"
  wc -l "${out_path}/${file1}"
  wc -l "${out_path}/${file2}"

  echo "Sort the dumped edges"
  sort -k 1,1n -k2,2n < "${out_path}/${file1}" > "${out_path}/tmp_sorted1"
  sort -k 1,1n -k2,2n < "${out_path}/${file2}" > "${out_path}/tmp_sorted2"

  echo "Compare the dumped edges"
  diff "${out_path}/tmp_sorted1" "${out_path}/tmp_sorted2" > ${out_path}/adj_diff
  num_diff=$(< ${out_path}/adj_diff wc -l)

  if [[ ${num_diff} -eq 0 ]]; then
    echo "<< Passed the test!! >>"
  else
    err "<< Failed the test!! >>"
    exit
  fi
  echo ""

  /bin/rm "${out_path}/${file1}" "${out_path}/${file2}" "${out_path}/tmp_sorted1" "${out_path}/tmp_sorted2" ${out_path}/adj_diff
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
  ./run_adj_list_bench_metall -o "${out_path}/metall_test_dir" -f ${file_size} -d "${out_path}/${out_name}" -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1
  check_program_exit_status
  echo ""

  ./edge_generator/generate_rmat_edge_list -o "${out_path}/${out_ref}" -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1
  check_program_exit_status
  echo ""

  compare ${out_name} ${out_ref}

  # ----- Reopen the file for persistence test----- #
  ./test/open_metall -o "${out_path}/metall_test_dir" -d "${out_path}/${out_name}"
  check_program_exit_status
  echo ""

  ./edge_generator/generate_rmat_edge_list -o "${out_path}/${out_ref}" -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1
  check_program_exit_status
  echo ""

  compare ${out_name} ${out_ref}

  /bin/rm -rf ${out_path}/metall_test_dir
}

main "$@"