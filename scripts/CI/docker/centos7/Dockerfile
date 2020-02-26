FROM centos:centos7
MAINTAINER Keita Iwabuchi <kiwabuchi@llnl.gov>

# Install required packages
RUN yum -y install bzip2 diffutils flex git openssl-devel make tar wget && \
    yum -y install gcc gcc-c++ && \
    yum -y install texinfo && \
    yum clean all --enablerepo=\* && \
    wget -q --no-check-certificate https://github.com/Kitware/CMake/releases/download/v3.16.4/cmake-3.16.4.tar.gz -O /tmp/cmake_src.tar.gz && \
    tar xf /tmp/cmake_src.tar.gz -C /tmp && cd /tmp/cmake-3.16.4 && \
    ./bootstrap > /dev/null && make -j8 > /dev/null && make install && \
    /bin/rm -rf /tmp/cmake*

# Download multiple versions of Boost to /var/boost
WORKDIR /var/boost
COPY download_boost.sh .
RUN chmod +x ./download_boost.sh && ./download_boost.sh

# Install GCC
WORKDIR ../../tmp/gcc_work_dir
COPY install_gcc.sh .
RUN sh install_gcc.sh && cd ../ && rm -rf gcc_work_dir

WORKDIR /