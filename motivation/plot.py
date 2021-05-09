#!/usr/bin/python
import sys
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.backends.backend_pdf import PdfPages

colors = ['#f24438', '#2298f2']
patterns = ['x', 'o']

def processData(filename):
    with open(filename) as f:
        content = f.readlines()
    content = [x.strip() for x in content]
    content = [x.split(",") for x in content]

    data = []
    for tokens in content:
        if len(tokens) < 3: continue
        data.append([tokens[0], tokens[2], tokens[3]])
    return data

pp = PdfPages("pmemkv-bw-limit.pdf")

xAxis = []
yAxis1 = []
yAxis2 = []
legends = []
yAxis1Max = 0
yAxis2Max = 0
for i in range(1, len(sys.argv)):
    data = processData(sys.argv[i])
    if len(xAxis) == 0:
        xAxis = [int(x[0]) for x in data]

    yAxis1.append([float(x[1]) / 1000 for x in data])
    Max = np.max(yAxis1[len(yAxis1) - 1])
    if Max > yAxis1Max:
        yAxis1Max = Max
    yAxis2.append([float(x[2]) for x in data])
    Max = np.max(yAxis2[len(yAxis2) - 1])
    if Max > yAxis2Max:
        yAxis2Max = Max

    tokens = sys.argv[i].split("-")
    legend = tokens[len(tokens) - 1].split(".")[0]
    legends.append('PMEMKV-' + legend.title())

N = len(xAxis)
np.arange(N)
fig = plt.figure(figsize=(6, 3))
ax1 = fig.add_subplot(111)
ax2 = ax1.twinx()
index = 0
for y in yAxis1:
    ax1.plot(xAxis, y, color=colors[index], zorder=3, marker=patterns[index])
    index = index + 1
index = 0
for y in yAxis2:
    ax2.plot(xAxis, y, color=colors[index], zorder=3, marker=patterns[index])
    index = index + 1

ubY1 = 0
while ubY1 < yAxis1Max:
    ubY1 = ubY1 + 200
ax1.set_yticks(np.arange(0, ubY1 + 1, step=ubY1/4))
ax1.set_xticks(xAxis)

ubY2 = 0
while ubY2 < yAxis2Max:
    ubY2 = ubY2 + 200
ax2.set_yticks(np.arange(0, ubY2 + 1, step=ubY2/4))

ax1.set_xlabel("Number of threads")
ax1.set_ylabel("Throughput (Kops/sec)")
ax2.set_ylabel("Bandwidth (MB/s)")
ax1.grid(True, zorder=0, linestyle='dashed')
art = []
lgd = plt.legend(legends, loc=9, bbox_to_anchor=(0.5, -0.25), ncol=3, frameon=False)
art.append(lgd)

plt.savefig(pp, format='pdf', additional_artists=art, bbox_inches="tight")
pp.close()
