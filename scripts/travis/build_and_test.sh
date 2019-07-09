#!/bin/bash

env
function or_die () {
    "$@"
    local status=$?
    if [[ $status != 0 ]] ; then
        echo ERROR $status command: $@
        exit $status
    fi
}

CMAKE_BASIC_FLAGS="-DBUILD_BENCH=ON -DBUILD_TEST=ON -DRUN_LARGE_SCALE_TEST=ON -DBUILD_DOC=OFF -DRUN_BUILD_AND_TEST_WITH_CI=ON"

cd ${BUILD_DIR}
# ls ./
# ls ${BOOST_ROOT}

BOOST_ROOT="${PWD}/boost_1_${BOOST_VERSION}_0"

if [[ "$DO_BUILD" == "yes" ]] ; then
    if [[ $IMG == *"clang"* ]]; then
        C_COMPILER=clang
        CXX_COMPILER=clang++
    else
        C_COMPILER=gcc
        CXX_COMPILER=g++
    fi

    for BUILD_TYPE in Debug Release RelWithDebInfo; do
        for USE_SPACE_AWARE_BIN in On Off; do
            or_die cmake -DCMAKE_C_COMPILER=${C_COMPILER} -DCMAKE_CXX_COMPILER=${CXX_COMPILER} -DBOOST_ROOT=${BOOST_ROOT} -DCMAKE_BUILD_TYPE=${BUILD_TYPE} -DUSE_SPACE_AWARE_BIN=${USE_SPACE_AWARE_BIN} ${CMAKE_BASIC_FLAGS} ../

            or_die make -j 8 # VERBOSE=1

            if [[ "${DO_TEST}" == "yes" ]] ; then
                or_die ctest -T test --output-on-failure -V
                cd bench/adjacency_lsit
                or_die bash ../../../bench/adjacency_lsit/test/test.sh
                # or_die ../../../bench/adjacency_lsit/test/test_large.sh
                cd ../../
            fi
        done
    done
fi

exit 0