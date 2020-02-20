FROM centos:centos8
# FROM centos:centos7
MAINTAINER Keita Iwabuchi <kiwabuchi@llnl.gov>

# Install required packages
RUN dnf -y install bzip2 cmake3 diffutils flex git make tar wget && \
    dnf -y install gcc gcc-c++ && \
    dnf -y --enablerepo=PowerTools install texinfo && \
    dnf clean all --enablerepo=\*

# Download multiple versions of Boost to /var/boost
WORKDIR /var/boost
COPY download_boost.sh .
RUN /bin/bash download_boost.sh

# Install GCC
WORKDIR ../../tmp/gcc_work_dir
COPY install_gcc.sh .
RUN /bin/bash install_gcc.sh && cd ../ && rm -rf gcc_work_dir

WORKDIR /