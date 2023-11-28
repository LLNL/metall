#!/bin/bash

# This script builds and runs example programs with various sanitizers.
# Usage example:
# cd metall # Metall root directory
# sh scripts/sanitizer/run_sanitizers.sh

# Change the following variables to your own settings
CC=clang
CXX=clang++
BOOST_ROOT="${HOME}/local/boost"
METALL_ROOT=".."

LOG_FILE=out-sanitizer.log

exec_cmd () {
  echo "" >> ${LOG_FILE}
  echo "------------------------------------------------------------" >> ${LOG_FILE}
  echo "Command: " "$@" | tee -a ${LOG_FILE}
  "$@" >> ${LOG_FILE} 2>&1
  echo "------------------------------------------------------------" >> ${LOG_FILE}
  echo "" >> ${LOG_FILE}
}

build () {
    local target=$1
    exec_cmd ${CXX} -I ${BOOST_ROOT} -I ${METALL_ROOT}/include -std=c++17 ${SANITIZER_BUILD_ARGS}  ${target} -DMETALL_DEFAULT_CAPACITY=$((1024*1024*500))
}

run_sanitizer () {
    SANITIZER_BUILD_ARGS=$1

#     build "${METALL_ROOT}/example/simple.cpp"
#     ./a.out 2>&1 | tee -a ${LOG_FILE}
#      rm -f a.out

    build "${METALL_ROOT}/example/concurrent.cpp"
    echo "Running concurrent program"
    ./a.out 2>&1 | tee -a ${LOG_FILE}
     rm -f a.out
}

main() {
    mkdir build
    cd build
    rm -rf ${LOG_FILE}

    echo ""
    echo "====================================================================="
    echo "Build with address sanitizer and leak sanitizer"
    run_sanitizer "-fsanitize=address -fno-omit-frame-pointer  -O1 -g"

    echo ""
    echo "====================================================================="
    echo "Build with thread sanitizer"
    run_sanitizer "-fsanitize=thread -fPIE -pie -O1 -g"

    echo ""
    echo "====================================================================="
    echo "Build with memory sanitizer"
    run_sanitizer "-fsanitize=memory -fsanitize-memory-track-origins -fPIE -pie -fno-omit-frame-pointer -g -O2"

    echo ""
    echo "====================================================================="
    echo "Build with undefined behavior sanitizer"
    run_sanitizer "-fsanitize=undefined  -O1 -g"
}

main "$@"