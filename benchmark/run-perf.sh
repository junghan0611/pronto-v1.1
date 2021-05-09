#!/bin/bash

ERR_LIMIT="0.05"
MNT_POINT=/mnt/ram
NR_HUGE_PAGES=$((8192 * 2 * 2)) # 64 GB
MAX_ITERATIONS=15
if [ $# -ne 0 ]; then
    MAX_ITERATIONS=$1
fi
CORES_PER_SOCKET=`lscpu | grep 'Core(s) per socket' | awk '{ print $4 }'`
MAX_THREADS=1 # only run single-threaded for STL and PMEMKV

# Run with jemalloc
PRELOAD=`jemalloc-config --libdir`/libjemalloc.so.`jemalloc-config --revision`

export LD_LIBRARY_PATH=../motivation/pmemkv-0.3x-vanilla/bin

make clean 1>/dev/null 2>&1 && make 1>/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    echo "Unable to create benchmark binary!"
    exit 1
fi

for MODE in "rocksdb" "pmemkv" "persistent" "persistent-sync" "volatile"; do
    if [ -z ${USE_SMALL_DATASET+x} ]; then
        TRACE_ROOT="./traces/normal"
    else
        TRACE_ROOT="./traces/micro"
    fi
    if [ "$MODE" == "persistent" ] || [ "$MODE" == "persistent-sync" ]; then
        echo $NR_HUGE_PAGES | sudo numactl --membind=0 --cpunodebind=0 tee -a /proc/sys/vm/nr_hugepages >/dev/null
    elif [ "$MODE" == "pmemkv" ] || [ "$MODE" == "rocksdb" ]; then
        echo 0 | sudo tee -a /proc/sys/vm/nr_hugepages >/dev/null
    else
        sudo umount $MNT_POINT 1>/dev/null 2>&1
    fi

    MAX_THREADS=1
    BENCHMARKS="ordered-map queue vector map hash-map"
    if [ "$MODE" == "rocksdb" ]; then
        BENCHMARKS="a-sync b-sync a-async b-async a-pronto b-pronto a-pronto-sync b-pronto-sync"
        MAX_THREADS=$CORES_PER_SOCKET
    elif [ "$MODE" == "pmemkv" ]; then
        BENCHMARKS="kvtree2"
    fi

    for BENCH in $BENCHMARKS; do
        THREADS=1
        SNAPSHOT_FREQ=15

        if [ "$BENCH" == "a-pronto" ]; then # non-pronto benchmarks must come prior
            numactl --membind=0 --cpunodebind=0 echo $NR_HUGE_PAGES | sudo tee -a /proc/sys/vm/nr_hugepages >/dev/null
        elif [ "$BENCH" == "hash-map" ]; then
            SNAPSHOT_FREQ=10
            MAX_THREADS=$CORES_PER_SOCKET
        fi

        while [[ $THREADS -le $MAX_THREADS ]]; do
            #for VALUE_SIZE in 256 512 1024 2048 4096; do
            for VALUE_SIZE in 256 512 1024 2048; do # skip 4~KB tests to reduce storage requirements
                if [ ! -z ${FIXED_VALUE_SIZE+x} ]; then
                    VALUE_SIZE=$FIXED_VALUE_SIZE
                fi

                if [ "$BENCH" == "hash-map-multi" ]; then
                    BENCH="hash-map"
                fi
                OPTION="${MODE}-${BENCH}"
                if [ "$MODE" == "persistent-sync" ]; then
                    OPTION="persistent-${BENCH}"
                fi
                if [ "$MODE" == "rocksdb" ]; then
                    WORKLOAD=${BENCH:0:1}
                    if [ -z ${USE_SMALL_DATASET+x} ]; then
                        TRACE="workloads/1m/$WORKLOAD"
                    else
                        TRACE="workloads/100k/$WORKLOAD"
                    fi
                else
                    TRACE="${TRACE_ROOT}/ycsb.${THREADS}."
                fi

                if [ "$MODE" == "persistent" ] || [ "$MODE" == "persistent-sync" ]; then
                    if [ "$BENCH" == "hash-map" ]; then
                        # Comment-out to disable per-bucket persistent objects
                        #BENCH="hash-map-multi"
                        BENCH=$BENCH
                    fi
                    OPS=`wc -l ${TRACE}0 | awk '{ print $1 }'`
                    # Build library
                    LOG_SIZE=$((($OPS * ($VALUE_SIZE + 64)) / 1024 / 1024 / 1024 + 6))
                    OPS=$(($OPS * $THREADS))
                    cd ../src
                    make clean 1>/dev/null 2>&1
                    if [ "$MODE" == "persistent" ]; then
                        unset PRONTO_SYNC
                    else # persistent-sync
                        export PRONTO_SYNC=1
                    fi
                    if [ "$BENCH" == "hash-map-multi" ]; then
                        LOG_SIZE=1
                    elif [ "$BENCH" == "hash-map" ]; then
                        LOG_SIZE=32
                    fi
                    LOG_SIZE=$LOG_SIZE make 1>/dev/null 2>&1
                    cd ../benchmark
                    make clean 1>/dev/null && BENCHMARK="nv${BENCH}" make 1>/dev/null 2>&1
                    if [[ $? -ne 0 ]]; then
                        break
                    fi
                    # Mount ext-4
                    MNT_SIZE=48
                    sudo umount $MNT_POINT 1>/dev/null 2>&1
                    ./../init_ext4.sh $MNT_SIZE
                elif [ "$MODE" == "pmemkv" ] || [ "$MODE" == "rocksdb" ]; then
                    cd ../src
                    make clean 1>/dev/null 2>&1
                    P_SYNC=0
                    if [ "$BENCH" == "a-pronto-sync" ] || [ "$BENCH" == "b-pronto-sync" ]; then
                        LOG_SIZE=16 PRONTO_SYNC=1 make 1>/dev/null 2>&1
                        P_SYNC=1
                    else
                        LOG_SIZE=16 make 1>/dev/null 2>&1
                    fi
                    cd ../benchmark
                    MNT_SIZE=48
                    OPTION=$MODE
                    sudo rm -f /mnt/ramdisk
                    sudo umount $MNT_POINT 1>/dev/null 2>&1
                    ./../init_ext4.sh $MNT_SIZE 1>/dev/null
                    sudo ln -s $MNT_POINT /mnt/ramdisk
                    make clean 1>/dev/null
                    RET=0
                    if [[ $P_SYNC -eq 1 ]]; then # RocksDB and Pronto + synchronous logging
                        MODE=$MODE BENCH=${BENCH:2:6} make 1>/dev/null 2>&1
                        RET=$?
                    else # Pronto (default)
                        MODE=$MODE BENCH=${BENCH:2} make 1>/dev/null 2>&1
                        RET=$?
                    fi
                    if [[ $RET -ne 0 ]]; then
                        break
                    fi
                else
                    make clean 1>/dev/null && MODE=$MODE BENCH=$BENCH make 1>/dev/null 2>&1
                    if [[ $? -ne 0 ]]; then
                        break
                    fi
                fi

                LATENCIES=(0 0 0 0 0)
                THROUGHPUTS=(100 200 300 400 500)

                ITERATION=0
                while [[ $ITERATION -lt $MAX_ITERATIONS ]]; do
                    if [ "$MODE" == "persistent" ]; then
                        ./snapshot.sh benchmark $SNAPSHOT_FREQ &
                        OUTPUT=`./benchmark $OPTION $TRACE $THREADS $VALUE_SIZE`
                    elif [ "$MODE" == "persistent-sync" ]; then
                        ./snapshot.sh benchmark $SNAPSHOT_FREQ &
                        OUTPUT=`numactl --membind=0 --cpunodebind=0 ./benchmark $OPTION $TRACE $THREADS $VALUE_SIZE`
                    elif [ "$MODE" == "pmemkv" ] || [ "$MODE" == "rocksdb" ]; then
                        OUTPUT=`PMEM_IS_PMEM_FORCE=1 numactl --membind=0 --cpunodebind=0 ./benchmark $OPTION $TRACE $THREADS $VALUE_SIZE`
                    else
                        OUTPUT=`export LD_PRELOAD=$PRELOAD && numactl --membind=0 --cpunodebind=0 ./benchmark $OPTION $TRACE $THREADS $VALUE_SIZE`
                    fi
                    if [[ $? -ne 0 ]]; then
                        >&2 echo $OUTPUT
                    else
                        LAT=`echo $OUTPUT | awk -F ',' '{ print $2 }'`
                        BW=`echo $OUTPUT | awk -F ',' '{ print $3 }'`

                        LATENCIES[4]=${LATENCIES[3]}
                        LATENCIES[3]=${LATENCIES[2]}
                        LATENCIES[2]=${LATENCIES[1]}
                        LATENCIES[1]=${LATENCIES[0]}
                        LATENCIES[0]=$LAT
                        THROUGHPUTS[4]=${THROUGHPUTS[3]}
                        THROUGHPUTS[3]=${THROUGHPUTS[2]}
                        THROUGHPUTS[2]=${THROUGHPUTS[1]}
                        THROUGHPUTS[1]=${THROUGHPUTS[0]}
                        THROUGHPUTS[0]=$BW

                        DATA="[ ${THROUGHPUTS[0]}, ${THROUGHPUTS[1]}, ${THROUGHPUTS[2]}, ${THROUGHPUTS[3]}, ${THROUGHPUTS[4]} ]"
                        STD=`echo -e "import numpy as np\nprint(np.std(${DATA}))\n" | python`
                        REF=`echo -e "import numpy as np\nprint(np.max(${DATA}) * ${ERR_LIMIT})\n" | python`
                        CMP=`python -c "print(${STD} <= ${REF})"`
                        if [ "$CMP" == "True" ]; then
                            ITERATION=$MAX_ITERATIONS
                        fi
                    fi
                    rm -rf ${MNT_POINT}/*
                    ITERATION=$(($ITERATION + 1))
                    sleep 1s
                done

                for I in `seq 0 4`; do
                    LAT=${LATENCIES[I]}
                    BW=${THROUGHPUTS[I]}
                    echo "$MODE,$BENCH,$THREADS,$VALUE_SIZE,$I,$LAT,$BW"
                done

                if [ ! -z ${FIXED_VALUE_SIZE+x} ]; then
                    break
                fi
            done
            if [ ! -z ${SINGLE_THREADED_ONLY+x} ]; then
                break
            fi
            THREADS=$(($THREADS * 2))
        done
    done
done

exit 0
