#!/bin/bash

# TODO refactor common configurations
ERR_LIMIT="0.05"
MNT_POINT=/mnt/ram
MAX_ITERATIONS=10

# Experiments for section 5.8
LOG_SIZE=16 # GB
MAX_DATA_SIZE=32
MNT_SIZE=$(($LOG_SIZE + $MAX_DATA_SIZE + 4)) # GB
sudo umount $MNT_POINT 1>/dev/null 2>&1
./../init_ext4.sh $MNT_SIZE 1>/dev/null
if [ $? -ne 0 ]; then
    echo 'Unable to mount the filesystem!'
    exit 1
fi

if [ -z ${SKIP_RECOVERY_TIME_TESTS+x} ]; then
    NR_HUGE_PAGES=$(($MAX_DATA_SIZE * 2 * 512)) # 2-MB pages
    echo $NR_HUGE_PAGES | sudo numactl --cpunodebind=0 tee -a /proc/sys/vm/nr_hugepages >/dev/null
    for MODE in 'sync' 'async'; do
        cd ../src
        make clean 1>/dev/null 2>&1
        if [ "$MODE" == 'async' ]; then
            LOG_SIZE=$LOG_SIZE make 1>/dev/null 2>&1
        else # async
            LOG_SIZE=$LOG_SIZE PRONTO_SYNC=1 make 1>/dev/null 2>&1
        fi
        cd ../benchmark
        make clean 1>/dev/null 2>&1 && make sort-vector 1>/dev/null 2>&1
        if [ $? -ne 0 ]; then
            echo 'Unable to build the benchmark!'
            exit 1
        fi
        for DATA_SIZE in 1 2 4 8 16 32; do
            for SNAPSHOT_FREQ in 2 4 8 16 32; do
                for THREADS in 1 2 4 8; do
                    LABEL="recovery-time,$MODE,$DATA_SIZE,$SNAPSHOT_FREQ,$THREADS"
                    TIMES=(0 0 0 0 0)
                    ITERATION=0
                    while [ $ITERATION -lt $MAX_ITERATIONS ]; do
                        rm -rf $MNT_POINT/*
                        numactl --cpunodebind=0 ./sort-vector $DATA_SIZE $SNAPSHOT_FREQ $THREADS
                        if [ $? -ne 0 ]; then
                            echo "Unable to run the data generation phase: $LABEL"
                            exit 1
                        fi
                        sleep 2s
                        OUTPUT=`numactl --cpunodebind=0 ./sort-vector | awk '{ print $4 }'`
                        if [ $? -ne 0 ]; then
                            echo "Unable to recover from snapshot: $LABEL"
                            exit 1
                        fi
                        TIME=`python -c "print(int($OUTPUT * 1000))"` # us
                        TIMES[4]=${TIMES[3]}
                        TIMES[3]=${TIMES[2]}
                        TIMES[2]=${TIMES[1]}
                        TIMES[1]=${TIMES[0]}
                        TIMES[0]=$TIME
                        DATA="[ ${TIMES[0]}, ${TIMES[1]}, ${TIMES[2]}, ${TIMES[3]}, ${TIMES[4]} ]"
                        STD=`echo -e "import numpy as np\nprint(np.std(${DATA}))\n" | python`
                        REF=`echo -e "import numpy as np\nprint(np.max(${DATA}) * ${ERR_LIMIT})\n" | python`
                        CMP=`python -c "print(${STD} <= ${REF})"`
                        if [ "$CMP" == "True" ]; then
                            ITERATION=$MAX_ITERATIONS
                        fi
                        ITERATION=$(($ITERATION + 1))
                        sleep 2s
                    done
                    OUTPUT=`echo -e "import numpy as np\nprint(np.average(${DATA}) / 1000)\n" | python`
                    echo "$LABEL,$OUTPUT"
                done
            done
        done
    done
else
    >&2 echo 'Skipping recovery-time tests!'
fi

# Experiments for section 5.7 (Figure 13)
export LD_LIBRARY_PATH=../motivation/pmemkv-0.3x-vanilla/bin
MAX_MODIFIED_PAGES=8192
if [ $# -ne 0 ]; then
    MAX_MODIFIED_PAGES=$1
fi
NR_HUGE_PAGES=$(($MAX_MODIFIED_PAGES + 256))
WRITE_RATIO=50
TIME_FACTOR=10 # Base-line execution time / Snapshot latency

cd ../src
make clean 1>/dev/null 2>&1
export LOG_SIZE=20 && make 1>/dev/null 2>&1
cd ../tools
make 1>/dev/null 2>&1
cd ../benchmark

make clean 1>/dev/null 2>&1 && make 1>/dev/null 2>&1
if [[ $? -ne 0 ]]; then
    echo "Unable to create binary!"
    exit 1
fi

sudo umount $MNT_POINT 1>/dev/null 2>&1
./../init_ext4.sh 40 1>/dev/null
echo $NR_HUGE_PAGES | sudo numactl --membind=0 --cpunodebind=0 tee -a /proc/sys/vm/nr_hugepages >/dev/null

for BENCH in "recovery-overhead-random" "recovery-overhead-sequential" "recovery-breakdown"; do
    PAGE_COUNT=1
    while [[ $PAGE_COUNT -le $MAX_MODIFIED_PAGES ]]; do

        OPERATIONS=10000000
        if [ "$BENCH" != "recovery-breakdown" ]; then # Calibrate
            RANDOM_ACCESS=0
            if [ "$BENCH" == "recovery-overhead-random" ]; then
                RANDOM_ACCESS=1
            fi

            BASE_LATENCY=0
            SNAPSHOT_LATENCY=0
            while [[ $BASE_LATENCY -eq 0 ]]; do
                OUTPUT=`./benchmark recovery-overhead $PAGE_COUNT $RANDOM_ACCESS $WRITE_RATIO $OPERATIONS`
                if [[ $? -eq 0 ]]; then
                    BASE_LATENCY=`echo $OUTPUT | awk -F ',' '{ print $2 }'`
                    SYNC_LATENCY=`./../tools/dump_snapshot ${MNT_POINT}/snapshot.1 | grep 'Sync' | awk '{ print $2 }'`
                    ASYNC_LATENCY=`./../tools/dump_snapshot ${MNT_POINT}/snapshot.1 | grep 'Async' | awk '{ print $2 }'`
                    SNAPSHOT_LATENCY=`python -c "print(int((${SYNC_LATENCY} + ${ASYNC_LATENCY}) * 1E6))"`
                fi
                rm -rf ${MNT_POINT}/*
                sleep 1s
            done

            # Number of operations = Operations (TIME_FACTOR x Snapshot-Latency / Base-Latency)
            OPERATIONS=`python -c "print($OPERATIONS * $TIME_FACTOR * $SNAPSHOT_LATENCY / $BASE_LATENCY)"`
            >&2 echo "$PAGE_COUNT: $OPERATIONS"
        fi

        V_ONE=(0 0 0)
        V_TWO=(0 0 0)
        ITERATION=0

        while [[ $ITERATION -lt $MAX_ITERATIONS ]]; do
            if [ "$BENCH" == "recovery-overhead-random" ]; then
                OUTPUT=`./benchmark recovery-overhead $PAGE_COUNT 1 $WRITE_RATIO $OPERATIONS`
            elif [ "$BENCH" == "recovery-overhead-sequential" ]; then
                OUTPUT=`./benchmark recovery-overhead $PAGE_COUNT 0 $WRITE_RATIO $OPERATIONS`
            else
                OUTPUT=`./benchmark $BENCH $PAGE_COUNT`
            fi
            if [[ $? -ne 0 ]]; then
                >&2 echo $OUTPUT
            else
                ONE=`echo $OUTPUT | awk -F ',' '{ print $2 }'`
                TWO=`echo $OUTPUT | awk -F ',' '{ print $3 }'`
                V_ONE[2]=${V_ONE[1]}
                V_ONE[1]=${V_ONE[0]}
                V_ONE[0]=$ONE
                V_TWO[2]=${V_TWO[1]}
                V_TWO[1]=${V_TWO[0]}
                V_TWO[0]=$TWO

                DATA="[ ${V_ONE[0]}, ${V_ONE[1]}, ${V_ONE[2]} ]"
                STD=`echo -e "import numpy as np\nprint(np.std(${DATA}))\n" | python`
                REF=`echo -e "import numpy as np\nprint(np.max(${DATA}) * ${ERR_LIMIT})\n" | python`
                CMP1=`python -c "print(${STD} <= ${REF})"`

                DATA="[ ${V_TWO[0]}, ${V_TWO[1]}, ${V_TWO[2]} ]"
                STD=`echo -e "import numpy as np\nprint(np.std(${DATA}))\n" | python`
                REF=`echo -e "import numpy as np\nprint(np.max(${DATA}) * ${ERR_LIMIT})\n" | python`
                CMP2=`python -c "print(${STD} <= ${REF})"`

                if [ "$CMP1" == "True" ] && [ "$CMP2" == "True" ]; then
                    ITERATION=$MAX_ITERATIONS
                else
                    >&2 echo "$PAGE_COUNT: error rate is too high <${V_ONE[*]} | ${V_TWO[*]}>"
                fi
            fi
            ITERATION=$(($ITERATION + 1))
            rm -rf ${MNT_POINT}/*
            sleep 1s
        done

        for I in `seq 0 2`; do
            echo "${BENCH},${PAGE_COUNT},${V_ONE[I]},${V_TWO[I]}"
        done

        PAGE_COUNT=$(($PAGE_COUNT * 2))
    done
done

exit 0
