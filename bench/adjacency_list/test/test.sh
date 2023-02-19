#!/bin/sh

# USAGE
# cd metall/build/bench/adjacency_list/
# sh ./test/test.sh

DATA=./test/data/edge_list_rmat_s10_
DATASTORE_DIR_ROOT="/tmp/metall_adj_test"

err() {
  echo "[$(date +'%Y-%m-%dT%H:%M:%S%z')]: $@" >&2
}

compare() {
  file1=$1
  file2=$2

  num_elements=$(< ${file1} wc -l)
  if [[ ${num_elements} -eq 0 ]]; then
    echo "<< ${file1} is empty!! >>"
    exit 1
  fi

  echo "Sort the dumped edges"
  sort -k 1,1n -k2,2n < ${file1} > ${DATASTORE_DIR_ROOT}/file1_sorted
  check_program_exit_status

  sort -k 1,1n -k2,2n < ${file2} > ${DATASTORE_DIR_ROOT}/file2_sorted
  check_program_exit_status

  echo "Compare the dumped edges"
  diff ${DATASTORE_DIR_ROOT}/file1_sorted ${DATASTORE_DIR_ROOT}/file2_sorted > ${DATASTORE_DIR_ROOT}/file_diff
  check_program_exit_status
  num_diff=$(< ${DATASTORE_DIR_ROOT}/file_diff wc -l)

  if [[ ${num_diff} -eq 0 ]]; then
    echo "<< Two edge lists are the same >>"
  else
    err "<< Two edge lists are different >>"
    err "<< Failed the test!! >>"
    exit 1
  fi
  echo ""

  /bin/rm -f ${DATASTORE_DIR_ROOT}/file1_sorted ${DATASTORE_DIR_ROOT}/file2_sorted ${DATASTORE_DIR_ROOT}/file_diff
}

check_program_exit_status() {
  local status=$?

  if [[ $status -ne 0 ]]; then
    err "<< The program did not finish correctly!! >>"
    err "<< Failed the test!! >>"
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

  echo "Create Test"
  ./run_adj_list_bench_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir -d ${DATASTORE_DIR_ROOT}/adj_out ${DATA}*
  check_program_exit_status
  rm -f ${DATASTORE_DIR_ROOT}/adj_ref
  cat ${DATA}* >> ${DATASTORE_DIR_ROOT}/adj_ref
  compare "${DATASTORE_DIR_ROOT}/adj_out" "${DATASTORE_DIR_ROOT}/adj_ref"
  /bin/rm -f "${DATASTORE_DIR_ROOT}/adj_out"

  # Open the adj-list with the open mode and add more edges
  echo ""
  echo "Extend Test"
  ./test/extend_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir -d ${DATASTORE_DIR_ROOT}/adj_out_extend ${DATA}*
  check_program_exit_status
  cat ${DATA}* >> ${DATASTORE_DIR_ROOT}/adj_ref
  compare "${DATASTORE_DIR_ROOT}/adj_out_extend" "${DATASTORE_DIR_ROOT}/adj_ref"

  # Open the adj-list with the read only open mode
  echo ""
  echo "Open Test"
  ./test/open_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir -d ${DATASTORE_DIR_ROOT}/adj_out_open
  check_program_exit_status
  compare "${DATASTORE_DIR_ROOT}/adj_out_open" "${DATASTORE_DIR_ROOT}/adj_ref"

  # Open the adj-list and destroy it to test memory leak
  echo ""
  echo "Destroy Test"
  ./test/destroy_metall -o ${DATASTORE_DIR_ROOT}/metall_test_dir
  check_program_exit_status

  echo "<< Passed all tests!! >>"

  #/bin/rm -rf ${DATASTORE_DIR_ROOT}
}

main "$@"