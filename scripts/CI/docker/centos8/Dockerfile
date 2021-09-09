FROM centos:centos8
MAINTAINER Keita Iwabuchi <kiwabuchi@llnl.gov>

# Install required packages
RUN dnf -y install git openssl-devel gcc gcc-c++ make patch bzip2.x86_64 cmake libarchive diffutils && \
    dnf clean all

# Install Spack
WORKDIR /var
RUN git clone https://github.com/spack/spack.git
WORKDIR /

# Install GCC
RUN . /var/spack/share/spack/setup-env.sh && \
    spack --insecure install gcc@11.2.0 languages=c,c++ && \
#    spack --insecure install gcc@11.1.0 languages=c,c++ && \
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
    spack --insecure install boost@1.77.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
#    spack --insecure install boost@1.76.0 ~atomic ~chrono ~clanglibcpp ~context ~coroutine ~date_time ~debug ~exception ~fiber ~filesystem ~graph ~icu ~iostreams ~locale ~log ~math ~mpi ~multithreaded ~numpy ~pic ~program_options ~python ~random ~regex ~serialization ~shared ~signals ~singlethreaded ~system ~taggedlayout ~test ~thread ~timer ~versionedlayout ~wave && \
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