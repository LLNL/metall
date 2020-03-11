#!/bin/sh

# USAGE
# cd metall/build/bench/adjacency_list/
# sh ../../../bench/adjacency_list/test/test.sh

DATA=../../../bench/adjacency_list/test/data/edge_list_rmat_s10_
DATASTORE_DIR_ROOT="/tmp"

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  num_elements=$(< ${file1} wc -l)
  if [[ ${num_elements} -eq 0 ]]; then
    echo "<< ${file1} is empty!! >>"
    exit
  fi

  echo "Sort the dumped edges"
  sort -k 1,1n -k2,2n < ${file1} > ${DATASTORE_DIR_ROOT}/file1_sorted
  check_program_exit_status

  sort -k 1,1n -k2,2n < ${file1} > ${DATASTORE_DIR_ROOT}/file2_sorted
  check_program_exit_status

  echo "Compare the dumped edges"
  diff ${DATASTORE_DIR_ROOT}/file1_sorted ${DATASTORE_DIR_ROOT}/file2_sorted > ${DATASTORE_DIR_ROOT}/file_diff
  check_program_exit_status
  num_diff=$(< ${DATASTORE_DIR_ROOT}/file_diff wc -l)

  if [[ ${num_diff} -eq 0 ]]; then
    echo "<< Passed the test!! >>"
  else
    err "<< Failed the test!! >>"
  fi
  echo ""

  /bin/rm -f ${DATASTORE_DIR_ROOT}/file1_sorted ${DATASTORE_DIR_ROOT}/file2_sorted ${DATASTORE_DIR_ROOT}/file_diff
}

check_program_exit_status() {
  local status=$?

  if [[ $status -ne 0 ]]; then
    err "<< The program did not finished correctly!! >>"
    exit $status
  fi
}

parse_option() {
  while getopts "d:" OPT
  do
    case $OPT in
      d) DATASTORE_DIR_ROOT=$OPTARG;;
      :) echo  "[ERROR] Option argument is undefined.";;
      \?) echo "[ERROR] Undefined options.";;
    esac
  done
}

main() {
  parse_option "$@"

  mkdir -p ${DATASTORE_DIR_ROOT}

  echo "CreateTest"
  ./run_adj_list_bench_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir -f $((2**26)) -d ${DATASTORE_DIR_ROOT}/adj_out ${DATA}*
  check_program_exit_status
  cat ${DATA}* >> ${DATASTORE_DIR_ROOT}/adj_ref
  compare "${DATASTORE_DIR_ROOT}/adj_out" "${DATASTORE_DIR_ROOT}/adj_ref"
  /bin/rm -f "${DATASTORE_DIR_ROOT}/adj_out" "${DATASTORE_DIR_ROOT}/adj_ref"

  echo ""
  echo "OpenTest"
  ./test/open_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir -d ${DATASTORE_DIR_ROOT}/adj_out_reopen
  check_program_exit_status
  cat ${DATA}* >> ${DATASTORE_DIR_ROOT}/adj_ref
  compare "${DATASTORE_DIR_ROOT}/adj_out_reopen" "${DATASTORE_DIR_ROOT}/adj_ref"

  /bin/rm -rf ${DATASTORE_DIR_ROOT}
}

main "$@"