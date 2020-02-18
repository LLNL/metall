#!/bin/sh

download_boost_headers() {
    INSTALL_DIR=${PWD}
    cd /tmp

    BOOST_MINOR_VER=$1
    BOOST_PATCH_VER=$2
    BOOST_DIR_NAME="boost_1_${BOOST_MINOR_VER}_${BOOST_PATCH_VER}"
    TAR_FILE_NAME="${BOOST_DIR_NAME}.tar.bz2"

    wget -q --no-check-certificate https://dl.bintray.com/boostorg/release/1.${BOOST_MINOR_VER}.${BOOST_PATCH_VER}/source/${TAR_FILE_NAME}
    tar -xf ${TAR_FILE_NAME}
    /bin/rm -rf ${TAR_FILE_NAME}

    mkdir ${INSTALL_DIR}/${BOOST_DIR_NAME}
    /bin/cp -r ${BOOST_DIR_NAME}/boost ${INSTALL_DIR}/${BOOST_DIR_NAME}/
    /bin/rm -rf /tmp/${BOOST_DIR_NAME}
    cd ${INSTALL_DIR}
}

main() {
    download_boost_headers 64 0
    download_boost_headers 65 0
    download_boost_headers 65 1
    download_boost_headers 66 0
    download_boost_headers 67 0
    download_boost_headers 68 0
    download_boost_headers 69 0
    download_boost_headers 70 0
    download_boost_headers 71 0
    download_boost_headers 72 0
}

main "$@"