## Running experiments inside Docker
Follow the steps below to create a Docker image for Pronto, and then run the experiments inside a container.

## Creating Pronto archive
Run the following command to create an archive from the Pronto repository.
The script that creates the Docker image uses this archive to install dependencies, build Pronto, and run its experiments.

Note that the Docker will run all experiments except for the recovery-time measurements (section 5.8 of the paper).
You can enable the recovery-time measurements by modifying the `run.sh` script in this directory, and before creating the archive.

```bash
./arxiv.sh
```

## Creating the Docker image
Run the following command to create the Docker image for the current version of Pronto.
Before running the command, make sure `Pronto.tar.gz` is present in this directory (or run `./arxiv.sh` to create it).
Creating the image might take a while (depending on your available resources).

```bash
docker build --tag=pronto .
```

## Creating a container from the Docker image
You must run the container in privileged mode and make sure the number of huge pages can be configured through */proc/sys/vm/nr_hugepages*. Also, the image runs the benchmarks on */dev/pmem1* and saves the benchmark results 
under */tmp*. To run the benchmark atop another NVMM device, make proper changes to *init.sh* or change the 
device mapping through the input arguments of the following command.

```bash
docker run --privileged -v /tmp:/tmp pronto
```

Once you create and run the container, it will run all the benchmarks and store the results under the */tmp*.
Note that running the container on our development machine approximately takes 7 hours to complete.
