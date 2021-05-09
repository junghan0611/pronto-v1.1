#!/bin/bash
apt-get -y install git-core

apt-get -y install numactl
apt-get -y install build-essential
apt-get -y install uuid-dev
apt-get -y install pkg-config
apt-get -y install libndctl-dev
apt-get -y install libdaxctl-dev
apt-get -y install autoconf
apt-get -y install cmake
apt-get -y install python
apt-get -y install curl
apt-get -y install libz-dev
apt-get -y install doxygen
apt-get -y install graphviz

curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
python get-pip.py
python -m pip install numpy

wget https://github.com/jemalloc/jemalloc/archive/5.1.0.tar.gz && \
    tar -xzvf 5.1.0.tar.gz && rm 5.1.0.tar.gz && \
    cd jemalloc-5.1.0 && ./autogen.sh && ./configure && make -j && \
    make install && cd ..

wget https://github.com/pmem/pmdk/archive/1.4.1.tar.gz && \
    tar -xzvf 1.4.1.tar.gz && rm 1.4.1.tar.gz && cd pmdk-1.4.1 && \
    make -j && make install && cd ..

exit 0
