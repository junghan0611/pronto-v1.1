## Debugging tool for the allocator
A simple tool to measure the performance of the allocator and report average time for `alloc` and `free`.

The tool can also generate a flame-graph for debugging/optimization purposes.

## Requirements
The allocator uses huge-pages internally. Make sure you reserve huge-pages using the command below before running the tool. 
Then you can use the make file to build the binary and run the test.

```bash
echo 1024 | sudo tee -a /proc/sys/vm/nr_hugepages # reserves 1024 huge-pages
```

## Generating a flame-graph
You can generate a flame-graph for the allocation sub-system by running `flamegraph.sh`.
This script requires downloading the *FlameGraph* repository from [here](https://github.com/brendangregg/FlameGraph), 
so make sure your machine can access this URL.
You can also configure the allocation unit (by default 64 bytes) by passing it as an argument to the script.
```bash
./flamegraph.sh # uses 64-byte as allocation unit
./flamegraph.sh 1024 # does 1 KB allocations
```
