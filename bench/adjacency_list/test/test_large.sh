#!/bin/sh

# USAGE
# cd metall/build/bench/adjacency_list/
# sh ./test/test_large.sh

# ----- Default Configuration ----- #
# Parameters to generate R-MAT graph
v=17 # The number of vertices in the graph is 2^SCALE
a=0.57
b=0.19
c=0.19
seed=123

# The default path to store data.
# This value is overwritten if '-d' option is specified
case "$OSTYPE" in
  darwin*)  out_dir_path="/tmp/metall_adj_test";;
  linux*)   out_dir_path="/dev/shm/metall_adj_test";;
esac
# --------------- #

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  echo "Compare the dumped edges"
  ./test/compare_key_value_lists ${file1} ${file2}
  check_program_exit_status
}

check_program_exit_status() {
  local status=$?

  if [[ $status -ne 0 ]]; then
    err "<< The program did not finish correctly!! >>"
    exit $status
  fi
}

parse_option() {
  while getopts "d:v:" OPT
  do
    case $OPT in
      d) out_dir_path=$OPTARG;;
      v) v=$OPTARG;;
      :) echo  "[ERROR] Option argument is undefined.";;
      \?) echo "[ERROR] Undefined options.";;
    esac
  done
}

show_system_info() {
  echo ""
  echo "--------------------"
  echo "System info"
  echo "--------------------"
  df -h
  df -ih
  free -g
  uname -r
  echo ""
}

main() {
  parse_option "$@"
  mkdir -p ${out_dir_path}

  data_store_path="${out_dir_path}/metall_test_dir"
  adj_list_dump_file="${out_dir_path}/dumped_edge_list"

  # Contains edges generated directly from the edge generator
  ref_edge_dump_file1="${out_dir_path}/ref_edge_list1"
  ref_edge_dump_file2="${out_dir_path}/ref_edge_list2"

  local e=$((2**$((${v}+4)))) # The number of edges to generate

  ./run_adj_list_bench_metall -o ${data_store_path} -d ${adj_list_dump_file} -s ${seed} -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1 -D ${ref_edge_dump_file1}
  check_program_exit_status
  echo ""

  compare ${adj_list_dump_file} ${ref_edge_dump_file1}
  echo ""

  /bin/rm -rf ${adj_list_dump_file}

  # ---------- Reopen the file for persistence test ---------- #
  # Open and add another edge list
  ./test/extend_metall -o ${data_store_path} -d ${adj_list_dump_file} -s $(($seed+1024)) -v ${v} -e ${e} -a ${a} -b ${b} -c ${c} -r 1 -u 1 -D ${ref_edge_dump_file2}
  check_program_exit_status
  echo ""

  # As the adjacency list contains the two edge lists, combine them
  cat ${ref_edge_dump_file1} >> ${ref_edge_dump_file2}

  compare ${adj_list_dump_file} ${ref_edge_dump_file2}
  echo ""

  /bin/rm -rf ${adj_list_dump_file}

  # Open the datastore as the read only mode
  ./test/open_metall -o ${data_store_path} -d ${adj_list_dump_file}
  check_program_exit_status
  echo ""

  compare ${adj_list_dump_file} ${ref_edge_dump_file2}
  echo ""

  # Open the adj-list and destroy it to test memory leak
  ./test/destroy_metall -o ${data_store_path}
  check_program_exit_status

  echo "<< Passed all tests!! >>"

  /bin/rm -f ${adj_list_dump_file} ${ref_edge_dump_file1}  ${ref_edge_dump_file2}
  /bin/rm -rf ${data_store_path}
}

main "$@"