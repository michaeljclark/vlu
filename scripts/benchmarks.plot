set term png size 1280,640
set auto x
set style data histogram
set style histogram cluster gap 2
set style fill solid border -1
set boxwidth 0.9
set xtic scale 0

set style line 101 lc rgb '#606060' lt 1 lw 1
set border ls 101

set ylabel "GiB/sec" offset 0,0,0
set yrange [0:7]

set ytics 0,0.5,10
set grid xtics ytics

set format y "%5.3f"

set datafile missing "?"

set output "images/benchmarks.png"
set title "VLU8 Performance"
plot 'data/benchmarks.dat' using \
	'random-8-encode':xtic(1) ti col, \
	'' u 'random-56-encode' ti col, \
	'' u 'random-mix-encode' ti col, \
	'' u 'random-8-decode' ti col, \
	'' u 'random-56-decode' ti col,	\
	'' u 'random-mix-decode' ti col