## Debugging tools
These tools enable debugging the semantic-logging, snapshot, and allocation sub-systems.
You can run `make` to build the tools -- assuming you have already installed all the dependencies.

## Semantic logging
This tool dumps detailed information about semantic logs (and their log enteries).
Pass the UUID of the object of interest and the tool will dump details of its semantic log.

```bash
./dump_log 3b24574c-e920-4066-8eec-92f9e4682702
```

## Snapshot
This tool provides a summary for a particular snapshot (e.g., time of creation, objects in the snapshot, and execution cost).
Pass the path to the snapshot file to the tool and it will print the summary.

```bash
./dump_snapshot /mnt/ram/snapshot.1
```

## Allocation
Check the documentation under *alloc_debug*.
