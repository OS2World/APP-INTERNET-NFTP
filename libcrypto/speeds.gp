set key left
set logscale xy
set xlabel "Bits"
set ylabel "Seconds"
set xrange [256:4096]

plot \
  "speeds.dat" using 1:2 title "DEC Alpha 266MHz, load=3" with lines, \
  "speeds.dat" using 1:3 title "Intel Celeron 400MHz" with lines, \
  "speeds.dat" using 1:4 title "Intel 386SX 33MHz" with lines, \
  "speeds.dat" using 1:5 title "Intel Pentium 180MHz" with lines, \
  "speeds.dat" using 1:6 title "DEC Alpha EV56" with lines, \
  "speeds.dat" using 1:7 title "Sun SPARCstation 5" with lines, \
  "speeds.dat" using 1:8 title "RS/6000" with lines, \
  "speeds.dat" using 1:9 title "AMD Duron 950MHz" with lines
