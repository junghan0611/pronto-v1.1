# Pronto: Easy and Fast Persistence for Volatile Data Structures
Pronto is a new NVM library that reduces the programming effort required to add persistence to volatile data structures.
It uses asynchronous semantic logging (ASL), which is generic enough to allow programmers to add persistence to the existing volatile data structure (e.g., C++ Standard Template Library containers) with very little programming effort.
ASL can also move most durability code off the critical path. For more information, check out our ASPLOS'20 submission.

## Hardware requirements
We used two configurations for the development and final measurements of Pronto.
In absence of access to real persistent memory (e.g., [Intel Optance DC Persistent Memory](https://www.intel.com/content/www/us/en/architecture-and-technology/optane-dc-persistent-memory.html)), 
you can use the development configuration to build and run the benchmarks.

### Evaluation setup
We have evaluated Pronto's performance using the following testbed.
The evaluations, however, only require 8 physical cores per socket, 50+ GB of persistent memory, and 50+ GB of DRAM.
The benchmarks (almost always) use only one socket, so you do not need access to more than one CPU on multi-socket systems to run the benchmarks.

|       | Model                                   | Frequency | Capacity                        |
|-------|-----------------------------------------|-----------|---------------------------------|
| CPU   | Intel Cascade Lake (engineering sample) | 2.2 GHz   | 12 physical cores per socket    |
| NVM   | Intel Optane DC Persistent Memory       | 2666 MHz  | 1.5 TB (6 x 256 GB) per socket  |
| DRAM  | Micron DDR4 DIMM                        |           | 192 GB (6 x 32 GB) per socket   |

### Development setup
You need a machine with more than 8 physical cores per socket and at least 100 GB of memory to run all experiments.
However, Pronto offers an option to run performance benchmarks with small traces, thus allowing the use of smaller machines.
You will need to reserve 50+GB of memory (on either socket) to emulate persistent memory ([see here for instructions](https://pmem.io/2016/02/22/pm-emulation.html)). 

### Note
Pronto's test scripts (`init_ext4.sh`) assume the emulated NVM device can be accessed through `/dev/pmem1`.
If this is not the case on your test machine, you can update `init_ext4.sh` accordingly.

Also, the library assumes the maximum number of CPU cores is 40 (i.e., 20 physical cores). 
If your machine has more cores, you should update the `MAX_CORES` constant in 
`src/savitar.hpp` accordingly before building the library.

## Source code hierarchy
Below you can find a short summary for each directory/file in the main directory.
Most directories include README files with more specific instructions.

- **benchmark**: includes the source code and scripts for evaluating Pronto.
- **docker**: in this directory, you can find instructions and resources on running Pronto (and its benchmarks) inside Docker containers.
- **motivation**: this directory includes the source code for the unmodified version of PMemKV, as well as its volatile version.
- **src**: Pronto's source code (run `make` inside this directory to build the library).
- **tools**: debugging tools for Pronto.
- **unit**: unit tests for Pronto (using Google's testing framework).
- **dependencies.sh**: this script installs almost every dependency required for running the benchmarks.
- **init_ext4.sh**: the script to mount an NVM-aware file-system (in this case ext4-dax) at */mnt/ram*. All benchmarks use this script to initialize the file-system.

There are two more directories in the repository (**gflags** and **googletest**), which are dependencies for the unit-tests.

## Build dependencies
We have tested Pronto on Ubuntu 18.04 and created a list of required dependencies for the test platform. 
Run `dependencies.sh` to build/install the necessary binaries for building Pronto and running the experiments.
To run the unit-tests (optional), you also need the **Google C++ Testing Framework** installed on the machine.
You can find the source code and dependencies for the test framework in *googletest* and *gflags*.

## Running benchmarks
Follow the commands below in the same order to run the performance and recovery benchmarks.
For more information on how to configure and understand the results of each benchmark, 
check out the README under `benchmark`.

```bash
cd src && make # makes the library
cd .. && cd benchmark
make # makes the benchmarks
./run-perf.sh # runs performance benchmarks
./run-recovery.sh # runs recovery benchmarks
cd sensitivity && ./run.sh # sensitivity analysis for semantic logging
cd ../..
```

## Running inside Docker
You can follow the instructions under `docker` to create a Docker image for Pronto.
Then you can use this image to create a container that runs all the benchmarks.

## Understanding the results
Below you can find the mapping between the benchmark scripts, the outputs of the Docker container, and the figures in the paper.
For more information about the benchmark and helper scripts, check the README file under `benchmark`.

| Script              | Docker output               | Helper script to parse    | Figures     |
|---------------------|-----------------------------|---------------------------|-------------|
| run-perf.sh         | /tmp/pronto-perf.csv        | helpers/parse-perf.py     | 7, 8, 9, 10 |
| run-recovery.sh     | /tmp/pronto-recovery.csv    | helpers/parse-recovery.py | 13          |
| sensitivity/run.sh  | /tmp/pronto-sensitivity.csv | N/A                       | 12          |
