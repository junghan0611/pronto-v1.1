#!/bin/bash
ACCESS_SIZE=64
if [ $# -gt 0 ]; then
    ACCESS_SIZE=$1
fi
export ALLOC_SIZE=$ACCESS_SIZE
make clean && make
if [[ ! -d FlameGraph ]]; then
    git clone https://github.com/brendangregg/FlameGraph
fi
cd FlameGraph
sudo perf record -g -- ../ckpt_alloc_test
sudo perf script | ./stackcollapse-perf.pl > out.perf-folded
./flamegraph.pl out.perf-folded > ../flame-graph.svg
