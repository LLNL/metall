FROM centos:centos7
MAINTAINER Keita Iwabuchi <kiwabuchi@llnl.gov>

# Install required packages
RUN yum -y install bzip2 diffutils flex git openssl-devel make tar wget gcc gcc-c++ texinfo patch&& \
    yum clean all --enablerepo=\* && \
    wget -q --no-check-certificate https://github.com/Kitware/CMake/releases/download/v3.20.2/cmake-3.20.2.tar.gz -O /tmp/cmake_src.tar.gz && \
    tar xf /tmp/cmake_src.tar.gz -C /tmp && cd /tmp/cmake-3.20.2 && \
    ./bootstrap > /dev/null && make -j8 > /dev/null && make install && \
    /bin/rm -rf /tmp/cmake*

# Install Spack
WORKDIR /var
RUN git clone https://github.com/spack/spack.git && \
    export PATH=/var/spack/bin:$PATH && \
    sh /var/spack/share/spack/setup-env.sh
WORKDIR /

# Install GCC
RUN . /var/spack/share/spack/setup-env.sh && \
    spack --insecure install gcc@11.1.0 languages=c,c++ && \
#    spack --insecure install gcc@10.3.0 languages=c,c++ && \
#    spack --insecure install gcc@10.2.0 languages=c,c++ && \
#    spack --insecure install gcc@10.1.0 languages=c,c++ && \
#    spack --insecure install gcc@9.3.0 languages=c,c++ && \
#    spack --insecure install gcc@9.2.0 languages=c,c++ && \
#    spack --insecure install gcc@9.1.0 languages=c,c++ && \
#    spack --insecure install gcc@8.3.0 languages=c,c++ && \
#    spack --insecure install gcc@8.2.0 languages=c,c++ && \
    spack --insecure install gcc@8.1.0 languages=c,c++ && \
    spack clean --all

# Install Boost
RUN . /var/spack/share/spack/setup-env.sh && \
    spack --insecure install boost@1.76.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.75.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.74.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.73.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.72.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.71.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.70.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.69.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.68.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.67.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.66.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.65.1 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.65.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
    spack --insecure install boost@1.64.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
    spack clean --all