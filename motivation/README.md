## Cost of failure-atomicity for PMemKV 
A simple benchmark to measure the throughput/bandwidth of running a write-only workload against multiple instances of PMemKV.
We run traces from YCSB-A (one million inserts) agains multiple instances of PMemKV, where each instance is local to a single thread.

### PMemKV-Vanilla
The unmodified PMemKV (v0.3x), which provides failure-atomicity.

### PMemKV-Volatile
The modified version of PMemKV with no logging and persistence.

### Running the benchmark
After installing all the dependencies for the project, run the following command to create `pmemkv-bw-limit.pdf`.

```bash
./run-throughput.sh
```

The script also dumps raw numbers in `output-pmemkv-0.3x-vanilla.txt` and `output-pmemkv-0.3x-volatile.txt`.
