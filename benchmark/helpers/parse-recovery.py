#!/usr/bin/python
import sys
import csv
import numpy as np

dataLatency = {
    'Synchronous': {},
    'Asynchronous': {},
}
dataOverhead = {
    'Random': {},
    'Sequential': {},
}

# Data format
# benchmark,number-of-pages,value-1,value-2
with open(sys.argv[1], 'rb') as f:
    reader = csv.reader(f)
    for row in reader:
        bench = row[0]
        pages = int(row[1])
        size = pages * 2
        v_one = float(row[2])
        v_two = float(row[3])

        if bench == "recovery-breakdown":
            if size not in dataLatency['Synchronous']:
                dataLatency['Synchronous'][size] = []
                dataLatency['Asynchronous'][size] = []
            dataLatency['Synchronous'][size].append(v_one * 1E3)
            dataLatency['Asynchronous'][size].append(v_two * 1E3)
        elif bench == "recovery-overhead-random":
            if size not in dataOverhead['Random']:
                dataOverhead['Random'][size] = []
            dataOverhead['Random'][size].append(v_two / v_one)
        elif bench == "recovery-overhead-sequential":
            if size not in dataOverhead['Sequential']:
                dataOverhead['Sequential'][size] = []
            dataOverhead['Sequential'][size].append(v_two / v_one)
        else:
            print("Unknown benchmark: " + bench)
            sys.exit(1)

# Aggregate iterations
for mode in dataLatency:
    for size in dataLatency[mode]:
        temp = dataLatency[mode][size]
        temp = float(np.median(temp))
        dataLatency[mode][size] = float("{0:.3f}".format(temp / 1E6))
for mode in dataOverhead:
    for size in dataOverhead[mode]:
        temp = dataOverhead[mode][size]
        dataOverhead[mode][size] = float("{0:.3f}".format(np.median(temp)))

# Print output
sys.stdout.write('dataLatency = ')
print(dataLatency)
sys.stdout.write('dataOverhead = ')
print(dataOverhead)
