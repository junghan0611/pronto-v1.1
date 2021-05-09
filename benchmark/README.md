## Pronto benchmarks
This directory includes tools and benchmarks for evaluating Pronto's performance.

### Performance benchmarks
The performance benchmarks include STL, PMemKV, and RocksDB benchmarks (see `persistent/`, `volatile/`, and `rocksdb/` for the source).
Run `run-perf.sh` to execute all performance benchmarks. Note that the script only reports performance numbers for 256, 512, 1024, 
and 2048 byte values. To also run experiments for 4096 byte values, uncomment line 59 and comment-out line 60 of the script.

We also provide options to only run a limited set of performance benchmarks, mainly to test the functionality of the code.
This limited set takes approximately 15 minutes to run on our development machine, and you can run it using the commands below.

```bash
# builds PMemKV library (dependency for performance benchmarks)
cd ../motivation/pmemkv-0.3x-vanilla
cp ../pmemkv_throughput.cc src/
make example
cd ../../benchmark

# configures and runs the performance benchmarks
export USE_SMALL_DATASET=1 # use only 100k operations (see traces and workloads)
export FIXED_VALUE_SIZE=1024 # only run experiments for 1 KB values
export SINGLE_THREADED_ONLY=1 # only run single threaded experiments
./run-perf.sh
```

### Recovery benchmarks
The source for recovery benchmarks includes `sort-vector.cpp`, `recovery/breakdown.hpp`, and `recovery/overhead.hpp`.
You can initiate the benchmarks by running `run-recovery.sh`.

Note that the recovery-time benchmark (section 5.8 of the paper) is computationally expensive.
You can skip this benchmark while running the recovery bencharks by running the below command.

```bash
SKIP_RECOVERY_TIME_TESTS=1 ./run-recovery.sh
```

### Sensitivy analysis
You can find the source code under `sensitivity/` and run the benchmark via `sensitivity/run.sh`.

### Tools
This includes helper scripts to analyze/parse raw data from benchmark scripts.

```bash
./helpers/parse-perf.py /tmp/pronto-perf.csv # parses the output of ./run-perf.sh
./helpers/parse-recovery.py /tmp/pronto-recovery.csv # parses the output of ./run-recovery.sh
```

### Traces and Workloads
These two directories contain traces from YCSB (workloads A and B).
Both `traces` and `workloads` directories include two sub-directories to allow developers run short- or long-running benchmarks:
- `traces/micro`: 100-K insert operations.
- `traces/normal`: 1 million insert operations.
- `workloads/100k`: both load and run phase for YCSB-A and YCSB-B (with 100-K key-value pairs).
- `workloads/1m`: the load and run phase for YCSB-A and YCSB-B with 1 million key-value pairs.

Update `run-perf.sh` to change the workload size.
