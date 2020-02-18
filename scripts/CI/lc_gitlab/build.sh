#!/bin/sh

# Takes a environmental variable CMAKE_EXTRA_OPTIONS

or_die () {
    "$@"
    local status=$?
    if [[ $status != 0 ]] ; then
        echo ERROR $status command: $@
        exit $status
    fi
}

main() {
    mkdir build
    cd build

    for BUILD_TYPE in Debug Release RelWithDebInfo; do
        or_die cmake -DCMAKE_BUILD_TYPE=${BUILD_TYPE} \
                     -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ \
                     -DBUILD_BENCH=ON -DBUILD_TEST=ON -DRUN_LARGE_SCALE_TEST=ON -DBUILD_DOC=OFF -DRUN_BUILD_AND_TEST_WITH_CI=ON -DBUILD_VERIFICATION=OFF -DVERBOSE_SYSTEM_SUPPORT_WARNING=ON \
                     ${CMAKE_EXTRA_OPTIONS}
                     ../
        or_die make -j
    done
}

main "$@"