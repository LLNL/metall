#!/bin/sh

mkdir ${BUILD_DIR}
cd ${BUILD_DIR}
wget -q --no-check-certificate https://gigenet.dl.sourceforge.net/project/boost/boost/1.${BOOST_VERSION}.0/boost_1_${BOOST_VERSION}_0.tar.bz2
tar -xjf boost_1_${BOOST_VERSION}_0.tar.bz2

ls -ls
ls -ls boost_1_${BOOST_VERSION}_0
