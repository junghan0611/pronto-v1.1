#!/bin/bash
MIN_LATENCY=100 # 100 ns
MAX_LATENCY=100000 # 100 us
#MIN_VALUE_SIZE=64
#MAX_VALUE_SIZE=8192
MIN_VALUE_SIZE=1024
MAX_VALUE_SIZE=1024

LOG_SIZE=16 # GB
MNT_POINT=/mnt/ram
MNT_SIZE=$(($LOG_SIZE + 4)) # GB

sudo umount $MNT_POINT 1>/dev/null 2>&1
./../../init_ext4.sh $MNT_SIZE 1>/dev/null
if [ $? -ne 0 ]; then
    echo 'Unable to mount the filesystem!'
    exit 1
fi

NR_HUGE_PAGES=1024
echo $NR_HUGE_PAGES | sudo numactl --cpunodebind=0 tee -a /proc/sys/vm/nr_hugepages 1>/dev/null

LATENCY=$MIN_LATENCY
while [ $LATENCY -le $MAX_LATENCY ]; do
    VALUE_SIZE=$MIN_VALUE_SIZE
    while [ $VALUE_SIZE -le $MAX_VALUE_SIZE ]; do
        for MODE in "sync" "async"; do
            # 1. Build Pronto (sync or async)
            pushd . 1>/dev/null
            cd ../../src
            make clean 1>/dev/null 2>&1
            if [ "$MODE" == "async" ]; then
                LOG_SIZE=$LOG_SIZE make 1>/dev/null 2>&1
            else
                LOG_SIZE=$LOG_SIZE PRONTO_SYNC=1 make 1>/dev/null 2>&1
            fi
            popd 1>/dev/null

            # 2. Build the binary
            make clean 1>/dev/null 2>&1 && make 1>/dev/null 2>&1
            if [ $? -ne 0 ]; then
                echo 'Unable to build the benchmark binary!'
                exit 1
            fi

            # 3. Run the benchmark
            TIMES=(0 0 0 0 0)
            export PMEM_IS_PMEM_FORCE=1
            for INDEX in `seq 0 4`; do
                rm -rf $MNT_POINT/*
                TEMP=`numactl --cpunodebind=0 ./benchmark $LATENCY $VALUE_SIZE | awk -F',' '{ print $4  }'`
                if [ $? -ne 0 ]; then
                    echo 'Benchmark returned a non-zero value on exit!'
                    exit 1
                fi
                TIMES[$INDEX]=$TEMP
            done

            # 4. Generate output
            DATA="[ ${TIMES[0]}, ${TIMES[1]}, ${TIMES[2]}, ${TIMES[3]}, ${TIMES[4]} ]"
            STD=`echo -e "import numpy as np\nprint('{0:.2f}'.format(np.std(${DATA})))\n" | python`
            AVG=`echo -e "import numpy as np\nprint('{0:.2f}'.format(np.average(${DATA})))\n" | python`
            echo "$LATENCY,$VALUE_SIZE,$MODE,$AVG,$STD"
        done
        VALUE_SIZE=$(($VALUE_SIZE * 2))
    done
    LATENCY=$(($LATENCY * 10))
done

exit 0
