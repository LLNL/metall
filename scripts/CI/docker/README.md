# Usage

* There are two Dockerfile files: one is based on CentOS 7; the other is based on CentOS 8.
* Please copy common/download_boost.sh and common/install_gcc.sh to ./centos7 or ./centos8 when one wants to build a docker image.


```
$ cp ./common/* ./centos7/
$ docker build -t metall_test:centos7 ./centos7
```