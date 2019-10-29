#!/usr/bin/python3

import re;
import argparse

# Benchmark     <dist>-<op> <dist>-<op> <dist>-<op> <dist>-<op>
# <name>        <rate>      <rate>      <rate>      <rate>

parser = argparse.ArgumentParser(description='Pivot benchmark data')
parser.add_argument('-m', '--markdown', nargs='?', const=True, default=False, help='markdown format')
args = parser.parse_args()

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

sep = '|' if args.markdown else ''

if args.markdown:
    print("|", end = '')
print("%-28s" % "Benchmark", end = sep)
for col in cols:
    print("%-20s" % col, end = sep)
print('')

if args.markdown:
    print("|:%27s" % ('-' * 27), end = sep)
    for col in cols:
        print("%19s:" % ('-' * 19), end = sep)
    print('')

for bench in benchs:
    name = bench.replace('_','-')
    if args.markdown:
        name = name.replace('-', '&#8209;')
        print("|", end = '')
    print("%-28s" % name, end = sep)
    for distop in cols:
        key = "%s-%s" % (bench, distop)
        if key in rates:
            print("%-20s" % rates[key], end = sep)
        else:
            print("%-20s" % "?", end = sep)
    print('')
