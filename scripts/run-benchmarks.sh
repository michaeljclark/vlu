#!/usr/bin/zsh

./build/vlu_bench print_header

# run each benchmark 25 times and output best result
for i in 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24; do
	./build/vlu_bench ${i} 25 1000 | sort | head -1
done