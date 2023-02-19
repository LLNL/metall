#######################################
# Executes a command.
# Dies (exit) the shell if the command has been failed.
# Arguments:
#   A command with its arguments to execute.
# Outputs: STDOUT and STDERR
#######################################
or_die () {
  echo ""
  echo "Command: " "$@"
  echo ">>>>>>>>>>"
  "$@"
  local status=$?
  if [[ $status != 0 ]] ; then
      echo ERROR $status command: "$@"
      exit $status
  fi
  echo "<<<<<<<<<<"
}

#######################################
# Executes a command.
# Arguments:
#   A command with its arguments to execute.
# Outputs: STDOUT and STDERR
#######################################
exec_cmd () {
  echo ""
  echo "Command: " "$@"
  echo ">>>>>>>>>>"
  "$@"
  echo "<<<<<<<<<<"
}

#######################################
# As multiple CI jobs could run on the same machine
# generate an unique test dir for each CI job
# Globals:
#   METALL_TEST_DIR (option, defined if not given)
# Outputs: STDOUT
#
# MEMO: setup a filesystem in local
# truncate --size 512m XFSfile
# mkfs.xfs -m crc=1 -m reflink=1 XFSfile
# mkdir XFSmountpoint
# mount -o loop XFSfile XFSmountpoint
# xfs_info XFSmountpoint
# cd XFSmountpoint
#######################################
setup_test_dir() {
  if [[ -z "${METALL_TEST_DIR}" ]]; then
    METALL_TEST_DIR="/tmp/${RANDOM}"
  fi

  # mkdir -p ${METALL_TEST_DIR} # Metall creates automatically if the directory does not exist
  echo "Store test data to ${METALL_TEST_DIR}"
}

#######################################
# Show some system information
# Outputs: STDOUT and STDERR
#######################################
show_system_info() {
  exec_cmd df -h
  exec_cmd df -ih
  exec_cmd free -g
  exec_cmd uname -r
}