#!/usr/bin/python
import sys
import csv
import numpy as np

# Map for benchmark names to chart titles
titleMap = {
    'queue': 'Priority Queue',
    'vector': 'Vector',
    'map': 'Unordered Map',
    'ordered-map': 'Map',
    'kvtree2': 'PMemKV',
    'hash-map': 'HashMap',
    # RocksDB
    'a-sync': 'A-Sync',
    'b-sync': 'B-Sync',
    'a-async': 'A-Async',
    'b-async': 'B-Async',
    'a-pronto': 'A-Pronto',
    'b-pronto': 'B-Pronto',
    'a-pronto-sync': 'A-Pronto-Sync',
    'b-pronto-sync': 'B-Pronto-Sync',
}

# Filter for thread to core ratio experiments
threadCoreRatioFilter = [ 0.5, 1.0, 2.0 ]
threadCoreRatioValueSize = 1024

rawData = {}
maxThreads = 0
# Data format
# mode,bench,threads,value-size,iteration,latency,bandwidth
with open(sys.argv[1], 'rb') as f:
    reader = csv.reader(f)
    for row in reader:
        mode = row[0]
        if mode not in rawData:
            rawData[mode] = {}
        bench = row[1]
        if bench not in rawData[mode]:
            rawData[mode][bench] = {}
        threads = int(row[2])
        if threads > maxThreads:
            maxThreads = threads
        if threads not in rawData[mode][bench]:
            rawData[mode][bench][threads] = {}
        valueSize = int(row[3])
        if valueSize not in rawData[mode][bench][threads]:
            rawData[mode][bench][threads][valueSize] = {}
            rawData[mode][bench][threads][valueSize]['latency'] = []
            rawData[mode][bench][threads][valueSize]['throughput'] = []
        # Skip iteration (row[4])
        latency = 0
        if len(row[5]) > 0:
            latency = int(row[5])
        rawData[mode][bench][threads][valueSize]['latency'].append(latency)
        throughput = 0
        if len(row[6]) > 0:
            throughput = int(row[6])
        rawData[mode][bench][threads][valueSize]['throughput'].append(throughput)

# Reduce iterations to a single value (using median)
for mode in rawData:
    for bench in rawData[mode]:
        for threads in rawData[mode][bench]:
            for valueSize in rawData[mode][bench][threads]:
                latencies = rawData[mode][bench][threads][valueSize]['latency']
                throughputs = rawData[mode][bench][threads][valueSize]['throughput']
                latency = np.median(latencies)
                throughput = np.median(throughputs)
                rawData[mode][bench][threads][valueSize]['latency'] = latency
                rawData[mode][bench][threads][valueSize]['throughput'] = throughput

# Latency: mode = { bench: [ value-size: latency ] }
for mode in rawData:
    data = {}
    for bench in rawData[mode]:
        title = titleMap[bench]
        data[title] = {}
        for valueSize in rawData[mode][bench][1]:
            tag = valueSize
            data[title][tag] = float("{0:.3f}".format(rawData[mode][bench][1][valueSize]['latency'] / 1000))
    sys.stdout.write(mode + 'Latency = ')
    print(data)

# Throughput: mode = { bench: [ value-size: throughput ] }
for mode in rawData:
    data = {}
    for bench in rawData[mode]:
        title = titleMap[bench]
        data[title] = {}
        for valueSize in rawData[mode][bench][1]:
            t = []
            for threads in rawData[mode][bench]:
                if threads == 8:
                    continue
                t.append(float("{0:.3f}".format(rawData[mode][bench][threads][valueSize]['throughput'])))
            tag = valueSize
            data[title][tag] = float("{0:.3f}".format(np.max(t) / 1E6))
    sys.stdout.write(mode + 'Throughput = ')
    print(data)

# Throughput: mode = { bench: [ threads * 2 / max(threads): throughput ] }
for mode in rawData:
    data = {}
    for bench in rawData[mode]:
        title = titleMap[bench]
        data[title] = {}
        for threads in rawData[mode][bench]:
            ratio = float(threads) * 2 / maxThreads
            if ratio not in threadCoreRatioFilter:
                continue
            t = rawData[mode][bench][threads][threadCoreRatioValueSize]
            data[title][ratio] = float("{0:.3f}".format(t['throughput'] / 1E6))
#    sys.stdout.write(mode + 'ThroughputRatio = ')
#    print(data)
