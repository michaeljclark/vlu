#!/usr/bin/python3

import re;

# Benchmark     <dist>-<op> <dist>-<op> <dist>-<op> <dist>-<op>
# <name>        <rate>      <rate>      <rate>      <rate>

rates = {}
benchs = []
cols = []
with open("data/benchmarks.md") as f:
    for s in f:
        m = [x.strip(' ') for x in filter(None, re.split("\|", s.strip()))]
        n = re.match("([\w\d_\-\/]+) (\w+) \(([\w\-]+)\)", m[0])
        if n:
            bench = n.group(1)
            bench = bench.replace('strtoull','snprintf')
            op = n.group(2)
            dist = n.group(3)
            rate = float(m[5])
            distop = "%s-%s" % (dist, op)
            key = "%s-%s-%s" % (bench, dist, op)
            if not distop in cols:
                cols.append(distop)
            if not bench in benchs:
                benchs.append(bench)
            rates[key] = rate

print("%-20s" % "Benchmark", end = '')
for col in cols:
    print("%-20s" % col, end = '')
print('')

for bench in benchs:
    print("%-20s" % bench.replace('_','-'), end = '')
    for distop in cols:
        key = "%s-%s" % (bench, distop)
        if key in rates:
            print("%-20s" % rates[key], end = '')
        else:
            print("%-20s" % "?", end = '')
    print('')
