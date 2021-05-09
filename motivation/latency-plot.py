#!/usr/bin/python
import os
import sys
import subprocess
import numpy
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

colors = [
    '#f24438',
    '#2298f2',
    '#8cc24a',
    '#ff5724',
    '#05abf2',
    '#e81e61',
    '#cedb39',
    '#653ab5',
    '#009485',
    '#ffc108',
    '#3e52b3',
    '#4cad50',
    '#ff9500',
]
colors = ['#169e83', '#f29811', '#28ad60', '#d15400', '#2a81b8', '#bf382c', '#8c44ab', '#2d3e4f', '#7e8b8c']
#colors = ['#01579B', '#0288D1', '#03A9F4', '#4FC3F7', '#B3E5FC']
#colors = ['#dcebca','#b2dbca','#89cccc','#62bfd1','#45aacc','#3e93bd','#3678ad','#2d5f9c','#24458c','#1d2d7d','#162063','#11184a']
#data = [
#    [44.288, 46.569, 50.661, 56.141, 62.443, 68.232, 72.641, 78.403],
#    [12.96, 12.59, 11.838, 10.712, 9.413, 8.25, 7.468, 4.047],
#    [7.041, 6.528, 5.7, 4.631, 3.377, 2.24, 1.377, 0.746],
#    [2.359, 2.21, 1.954, 1.604, 1.165, 0.777, 0.491, 0.278],
#    [29.167, 28.204, 26.307, 24.004, 21.338, 18.866, 16.872, 15.788],
#    [4.185, 3.899, 3.54, 2.908, 2.264, 1.635, 1.151, 0.738]
#]

data = [
    [48.473, 50.468, 54.201, 59.049, 64.707, 69.867, 73.792, 79.141],
    [51.527, 49.532, 45.799, 40.951, 35.293, 30.133, 26.208, 20.859]
]

valueSizes = [64, 128, 256, 512, 1024, 2048, 4096, 8192]
#legends = ['Tree Management', 'Persistent Alloc', 'Logging', 'Get Direct Ptr' ,'Tx Management', 'Other']
legends = ['B-Tree Management', 'Failure-Atomicity']

def saveStackedPlot(title, xAxis, data, legends, output):
    # DEBUG
    print(data)
    # DEBUG
    pp = PdfPages(output)
    plt.figure(figsize=(6, 2)) # Added
    N = len(xAxis)
    ind = numpy.arange(N)
    width = 0.35

    bars = []
    bottoms = [0 for t in xAxis]
    for category in data:
        myColor = colors[len(bars)]
        #if len(data) < len(colors) / 2:
        #    myColor = colors[len(bars) * 2]
        bars.append(plt.bar(ind, category, width, bottom=bottoms, color=myColor, zorder=3))
        bottoms = [bottoms[i] + category[i] for i in range(0, len(xAxis))]

    plt.ylabel('Percentage of\ntotal execution time', multialignment='center')
    plt.xlabel('Value size (bytes)')
    #plt.title(title)
    plt.xticks(ind, xAxis)
    plt.yticks(numpy.arange(0, 101, 25))
    art = []
    lgd = plt.legend([p[0] for p in bars], legends, loc=9, bbox_to_anchor=(0.45, -0.18), ncol=2, frameon=False)
    art.append(lgd)
    plt.grid(True, zorder=0, linestyle='dotted')
    #plt.subplots_adjust(bottom=0)
    plt.savefig(pp, format='pdf', additional_artists=art, bbox_inches="tight")
    pp.close()


saveStackedPlot('Latency breakdown of PMEMKV', valueSizes, data, legends, 'pmemkv-latency-breakdown.pdf')
