#!/bin/bash
DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo -e "$DATE\tStarted running the benchmarks"

export LD_LIBRARY_PATH=/usr/local/lib
export PMEM_IS_PMEM_FORCE=1

{
cd /app/Pronto/benchmark

# Figures 7, 8, 9, and 10 of the paper
./run-perf.sh 1>/tmp/pronto-perf.csv
DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo -e "$DATE\tFinished running performance benchmarks"
sleep 10s

# Figure 13 of the paper
SKIP_RECOVERY_TIME_TESTS=1 ./run-recovery.sh 1>/tmp/pronto-recovery.csv
DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo -e "$DATE\tFinished running recovery benchmarks"
sleep 10s

# Figure 11
# Experiments are redacted (Microsoft proprietary)

# Figure 12 of the paper
cd sensitivity
./run.sh 1>/tmp/pronto-sensitivity.csv
DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo -e "$DATE\tFinished running sensitivity analysis"
} 2>/tmp/stderr

DATE=`date '+%Y-%m-%d %H:%M:%S'`
echo -e "$DATE\tFinished running the benchmarks"

exit 0
