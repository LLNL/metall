#!/bin/sh

download_boost() {
    BOOST_MINOR_VER=$1
    BOOST_PATCH_VER=$2
    TAR_FILE_NAME="boost_1_${BOOST_MINOR_VER}_${BOOST_PATCH_VER}.tar.bz2"
    wget -q --no-check-certificate https://dl.bintray.com/boostorg/release/1.${BOOST_MINOR_VER}.${BOOST_PATCH_VER}/source/${TAR_FILE_NAME}
    tar -xf ${TAR_FILE_NAME}
    rm -rf ${TAR_FILE_NAME}
}

main() {
    download_boost 64 0
    download_boost 65 0
    download_boost 65 1
    download_boost 66 0
    download_boost 67 0
    download_boost 68 0
    download_boost 69 0
    download_boost 70 0
    download_boost 71 0
    download_boost 72 0
}

main "$@"