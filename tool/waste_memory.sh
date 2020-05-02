#!/bin/bash

usage () {
  echo "Usage:"
  echo "$0 Size — Size is the number of GB to waste."
  exit 1
}

or_die () {
  echo "===================="
  echo "Command: " "$@"
  echo "----------"

  "$@"
  local status=$?
  if [[ $status != 0 ]] ; then
      echo ERROR $status command: "$@"
      exit $status
  fi

  echo "===================="
}

show_mem_usage () {
  or_die free -h
  or_die numactl --hardware
}

waste_memory () {
  echo "Wasting $waste_size_gb GB of memory"

  or_die numactl --all --interleave=all dd if=/dev/zero of=/dev/shm/${waste_size_gb}GB bs=4096 count=$((${waste_size_gb}*256*1024))
}

main() {
  if [ $# -ne 1 ]; then
    echo "Bad argument count: $#"
    usage
  fi

  waste_size_gb=$1

  waste_memory
  show_mem_usage
}

main "$@"