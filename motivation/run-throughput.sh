#!/bin/bash

VALUE_SIZE=1024
MNT_POINT=/mnt/ram
POOL_NAME=pmemkv
TRACE_PATH="../traces/ycsb-a.txt"
POOL_SIZE=16
DB_SIZE=2
CORES=`cat /proc/cpuinfo | grep 'processor' | wc -l`
FREE_MEMORY=`numastat -c -m | grep 'MemFree' | awk '{ print $2 }'` # MBs
FREE_MEMORY=$(($FREE_MEMORY / 1024))

prepare() {
    wget https://github.com/pmem/pmdk/archive/1.4.tar.gz 1>/dev/null 2>&1
    tar -xzvf 1.4.tar.gz 1>/dev/null 2>&1
    rm -f 1.4.tar.gz
    cd pmdk-1.4
    make -j 1>/dev/null 2>&1
    sudo make install 1>/dev/null 2>&1
    cd ..
    MOUNT_POINT=$1
    POOL_SIZE=$(($CORES * $DB_SIZE))
    if [ $POOL_SIZE -gt $FREE_MEMORY ]; then
        POOL_SIZE=2
        while [ $POOL_SIZE -lt $FREE_MEMORY ]; do
            POOL_SIZE=$(($POOL_SIZE + 2))
        done
        POOL_SIZE=$(($POOL_SIZE - 2))
    fi
    mount | grep "$MOUNT_POINT" > /dev/null || (sudo mkdir -p $MOUNT_POINT && sudo mount -t tmpfs -o size=${POOL_SIZE}G,mpol=bind:0 tmpfs $MOUNT_POINT)
    #mount | grep "$MOUNT_POINT" > /dev/null || (sudo mkdir -p $MOUNT_POINT && sudo mount -t tmpfs -o size=${POOL_SIZE}G tmpfs $MOUNT_POINT)
    POOL_SIZE=`df -alh | grep $MOUNT_POINT | awk '{ print $2 }'`
    POOL_SIZE=${POOL_SIZE:0:-1}
}

build() {
    ROOT_DIR=$1
    CUR_DIR=`pwd`
    cp pmemkv_throughput.cc $ROOT_DIR/src/
    cd $ROOT_DIR
    make throughput
    if [ $? -ne 0 ]; then
        exit 1
    fi
    cd $CUR_DIR
}

benchmark() {
    ROOT_DIR=$1
    THREADS=$2
    CUR_DIR=`pwd`
    cd $ROOT_DIR
    if [ $POOL_SIZE -ge $(($DB_SIZE * $THREADS)) ]; then
        OUTPUT=''
        for REP in `seq 1 5`; do
            rm -f $MNT_POINT/$POOL_NAME*
            TEMP=`PMEM_IS_PMEM_FORCE=1 numactl -m 0 -N 0 ./bin/pmemkv_throughput \
                $TRACE_PATH $MNT_POINT/$POOL_NAME $DB_SIZE $VALUE_SIZE $THREADS "kvtree2"`
            if [ "${#OUTPUT}" -eq 0 ]; then
                OUTPUT=$TEMP
                continue
            fi
            T_OLD=`echo $OUTPUT | cut -d',' -f3 | cut -d'.' -f1`
            T_NEW=`echo $TEMP | cut -d',' -f3 | cut -d'.' -f1`
            if [ "$T_NEW" -gt "$T_OLD" ]; then
                OUTPUT=$TEMP
            fi
        done
        echo $OUTPUT
    fi
    cd $CUR_DIR
}

if [[ ! -z "${BUILD_PMEMKV}" ]]; then
    build "$BUILD_PMEMKV"
    exit $?
fi

prepare $MNT_POINT
INPUT=""
for VERSION in 'pmemkv-0.3x-volatile' 'pmemkv-0.3x-vanilla'; do
    build $VERSION
    if [ $? -ne 0 ]; then
        echo "Failed to build benchmark for: $VERSION"
    fi
    rm -f output-$VERSION.txt
    for THREADS in `seq 2 2 $CORES`; do
        benchmark $VERSION $THREADS >> output-$VERSION.txt
    done
    INPUT="$INPUT output-$VERSION.txt"
done

./plot.py $INPUT

exit 0
