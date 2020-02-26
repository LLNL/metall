#!/bin/bash

build_gcc() {
    VERSION=$1
    wget -q --no-check-certificate http://www.netgull.com/gcc/releases/gcc-${VERSION}/gcc-${VERSION}.tar.xz
    tar Jxf gcc-${VERSION}.tar.xz
    cd gcc-${VERSION}
    ./contrib/download_prerequisites
    cd ../
    mkdir ./gcc_build_${VERSION}
    cd ./gcc_build_${VERSION}
    ../gcc-${VERSION}/configure --enable-languages=c,c++ --disable-multilib --program-suffix=${VERSION}
    make -j4  > /dev/null
    # make test  > /dev/null
    make install
    cd ../
}

main() {
    build_gcc "8.1.0" # Might fail with CentOS 8
    build_gcc "8.2.0" # Might fail with CentOS 8
    build_gcc "8.3.0"
    build_gcc "9.1.0"
    build_gcc "9.2.0"
}

main "$@"